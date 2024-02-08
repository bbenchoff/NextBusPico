#include <SPI.h>
#include <EthernetLarge.h>
#include <SSLClient.h>
#include "trust_anchors.h"
#include "miniz.h"
#include <ArduinoUniqueID.h>
#include <NTPClient.h>
#include <EthernetUdp.h>
#include <TimeLib.h>
#include "Globals.h"

String APIkey = "da03f504-fc16-43e7-a736-319af37570be";
String stopCodes[] = {"16633"};

void decompressGzippedData(const uint8_t *gzippedData, size_t gzippedDataSize);
uint32_t getUncompressedLength(const uint8_t* data, size_t dataSize);
time_t iso8601ToEpoch(String datetime);
String extractExpectedArrivalTime(String data, time_t currentTime);
void getData(void);
void generateMAC(void);

void setup() {

  generateMAC();
  Ethernet.init(17); // 17 is specific to the W5500-P
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  delay(2000); //a non-blocking way for the Serial to connect.

  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with DHCP:");
  Serial.println("Mac Address: " + MACAddress);
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      while (true) {
        delay(1); // do nothing, no point running without Ethernet hardware
      }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // try to configure using IP address instead of DHCP:
    Ethernet.begin(mac, ip, myDns);
  } 
  else {
        Serial.print("DHCP assigned IP ");
  }
  Serial.println(Ethernet.localIP());
  // give the Ethernet shield a second to initialize:
  delay(1000);

  //Open a UDP port and begin the Time client.
  Udp.begin(8888);
  timeClient.begin();
}
  
void loop() {

  //We go through everything once, then repeat every minute
  timeClient.update();
  time_t currentTime = timeClient.getEpochTime();
  Serial.println(timeClient.getFormattedTime());
  getData(stopCodes[0]);
  Serial.println(extractExpectedArrivalTime(globalUncompressedDataStr, currentTime));

  while(true)
  {
    if(now() >= oneMinute+65)
    {
      timeClient.update();
      time_t currentTime = timeClient.getEpochTime();
      oneMinute = now();

      Serial.println(timeClient.getFormattedTime());
      getData(stopCodes[0]);
      Serial.println(extractExpectedArrivalTime(globalUncompressedDataStr, currentTime));
      Serial.println("");
      Serial.println("");
    }
  }
}

time_t iso8601ToEpoch(String datetime) {
    tmElements_t tm;

    int Year, Month, Day, Hour, Minute, Second;
    sscanf(datetime.c_str(), "%d-%d-%dT%d:%d:%dZ", &Year, &Month, &Day, &Hour, &Minute, &Second);

    tm.Year = Year - 1970; // tmElements_t.Year is the number of years since 1970
    tm.Month = Month;
    tm.Day = Day;
    tm.Hour = Hour;
    tm.Minute = Minute;
    tm.Second = Second;

    return makeTime(tm); // Converts to time_t
}



String extractExpectedArrivalTime(String data, time_t currentTime) {
  String searchKey = "\"ExpectedArrivalTime\":\"";
  int startIndex = 0;
  int endIndex = 0;
  String result = "";

  // Search for all occurrences of the key
  startIndex = data.indexOf(searchKey, startIndex);
  while (startIndex != -1) {
    // Adjust startIndex to get the actual starting position of the value
    startIndex += searchKey.length();

    // Find the end of the value
    endIndex = data.indexOf("\"", startIndex);

    // Extract the value
    String arrivalTimeISO8601 = data.substring(startIndex, endIndex);
    time_t arrivalEpoch = iso8601ToEpoch(arrivalTimeISO8601);

    // Calculate difference in minutes
    long minutesUntilArrival = (arrivalEpoch - currentTime) / 60;

    // Add extracted value to the result string
    if (result.length() > 0) {
      result += ", ";  // Separate multiple values with a comma
    }
    result += String(minutesUntilArrival) + " minutes";

    // Prepare for the next iteration
    startIndex = data.indexOf(searchKey, endIndex);
  }

  return result;
}

void getData(String stopCode)
{
  const int maxRetries = 3; // Maximum number of connection retries
  int retryCount = 0;
  bool connected = false;
  String line;
  bool isGzip = false;

  // Attempt to connect with retries
  while (!connected && retryCount < maxRetries) {
    if (client.connect(server, 443)) {
      connected = true;
    } else {
      Serial.println("Connection attempt failed, retrying...");
      retryCount++;
      delay(2000); // Wait 2 seconds before retrying
    }
  }

  if (!connected) {
    Serial.println("Failed to connect after retries, exiting function.");
    return; // Exit if still not connected after retries
  }

  // Send the HTTP GET request
  client.println("GET /transit/StopMonitoring?api_key=" + APIkey + "&agency=SF&stopCode=" + stopCode + "&format=json HTTP/1.1");
  client.println("User-Agent: " + User_Agent);
  client.println("Host: " + String(server_host));
  client.println("Connection: close");
  client.println(); // End of headers

  // Wait for response or timeout
  unsigned long startTime = millis();
  while (!client.available()) {
    if (millis() - startTime > 5000) { // 5-second timeout
      Serial.println("Timeout waiting for server response.");
      client.stop();
      return;
    }
  }

  // Read the HTTP status line
  String statusLine = client.readStringUntil('\n');
  Serial.println(statusLine); // Log the status line for debugging

  // Read and discard headers, looking for the end of the headers section
  bool headersFinished = false;
  while (client.available() && !headersFinished) {
    String headerLine = client.readStringUntil('\n');
    if (headerLine == "\r") {
      headersFinished = true;
    }
  }

  // Read the body
  while (client.connected() || client.available()) {
    while (client.available()) {
      byte buffer[2048]; // Adjust buffer size as necessary
      size_t bytesRead = client.read(buffer, sizeof(buffer));
      if (bytesRead > 0) {
        // Ensure we do not overflow the gzippedResponse buffer
        size_t toCopy = min(bytesRead, sizeof(gzippedResponse) - responseIndex);
        memcpy(gzippedResponse + responseIndex, buffer, toCopy);
        responseIndex += toCopy;
      }
    }
    if (!client.connected()) {
      break; // Exit the loop if the client has disconnected
    }
  }

  // Process the response if any data was received
  if (responseIndex > 0) {
    decompressGzippedData(gzippedResponse, responseIndex);
    responseIndex = 0; // Reset for next use
  }

  client.stop(); // Ensure the client is stopped
  Serial.println("Disconnected from server.");

}

void decompressGzippedData(const uint8_t *gzippedData, size_t gzippedDataSize) {
    Serial.print("Attempting to decompress data of size: ");
    Serial.println(gzippedDataSize);

    // Use the getUncompressedLength function to determine the expected uncompressed size.
    uint32_t expectedUncompressedSize = getUncompressedLength(gzippedData, gzippedDataSize);
    uint8_t *uncompressedData = (uint8_t *)malloc(expectedUncompressedSize);

    if (uncompressedData == NULL) {
        Serial.println("Failed to allocate memory for decompression.");
        return;
    }

    // Adjusting the pointer and size to skip the gzip header.
    // Note: This simple approach assumes no extra fields in the header. Adjust if needed.
    const size_t gzipHeaderSize = 10;
    const uint8_t *deflateData = gzippedData + gzipHeaderSize;
    size_t deflateDataSize = gzippedDataSize - gzipHeaderSize - 8; // Subtract 8 bytes for the gzip footer

    mz_ulong outBytes = expectedUncompressedSize;
    int status = tinfl_decompress_mem_to_mem(uncompressedData, outBytes, deflateData, deflateDataSize, 0);

    if (status == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
        Serial.println("Decompression failed.");
    } else {
        Serial.println("Decompression successful.");
        // Clear the global String before use
        globalUncompressedDataStr = "";

        // Convert uncompressed data to String
        for (size_t i = 0; i < outBytes; ++i) {
            globalUncompressedDataStr += (char)uncompressedData[i];
        }
        
        Serial.write(uncompressedData, outBytes);
        // print the String or its size
        //Serial.println("\nUncompressed Data Size: " + String(globalUncompressedDataStr.length()));
    }

    free(uncompressedData);
}

uint32_t getUncompressedLength(const uint8_t* data, size_t dataSize) {
    if (dataSize < 4) return 0; // Not enough data to contain the uncompressed size

    // The uncompressed size is stored in the last 4 bytes of the gzip stream in little-endian format
    uint32_t uncompressedSize = data[dataSize - 4]
        | (data[dataSize - 3] << 8)
        | (data[dataSize - 2] << 16)
        | (data[dataSize - 1] << 24);

    return uncompressedSize;
}

void generateMAC()
{
  // Read unique ID string, write to uniqueIDString
  for (size_t i = 0; i < 8; i++) {
    if (UniqueID8[i] < 0x10) {
      uniqueIDString += "0";
    }
    uniqueIDString += String(UniqueID8[i], HEX);
  }

  // Use the unique ID to set the MAC address
  if (uniqueIDString.length() >= 12) {
  // Loop through the last 6 bytes of the string
    for (size_t i = 0; i < 6; i++) {
      // Extract two characters (one byte in hex)
      String hexByteStr = uniqueIDString.substring(uniqueIDString.length() - 12 + i * 2, uniqueIDString.length() - 10 + i * 2);
      
      // Convert the hex string to a byte
      byte hexByte = (byte) strtol(hexByteStr.c_str(), NULL, 16);

      // Assign this byte to the mac array
      mac[i] = hexByte;
    }
  }

  for (int i = 0; i < 6; i++) {
    // Convert the byte to a hexadecimal string
    if (mac[i] < 16) {
        // Adding leading zero for bytes less than 16 (0x10)
        MACAddress += "0";
    }
    MACAddress += String(mac[i], HEX);
  }
  MACAddress.toUpperCase();
}