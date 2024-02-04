#include <SPI.h>
#include <EthernetLarge.h>
#include <SSLClient.h>
#include "trust_anchors.h"
#include "miniz.h"
#include <ArduinoUniqueID.h>

#define RESPONSE_BUFFER_SIZE 2048 // Adjust based on your expected response size
#define ETHERNET_LARGE_BUFFERS

String APIkey = "da03f504-fc16-43e7-a736-319af37570be";
String stopCodes[] = {"16633"};

// This uses ArduinoUniqueID to pull the serial number off of the Flash chip.
// Then, the least significant 6 bytes of the serial are used for the MAC.
byte mac[] = {};
String uniqueIDString = "";
String MACAddress = "";

const char server[] = "api.511.org";
const char server_host[] = "api.511.org";

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 177);
IPAddress myDns(8, 8, 8, 8);

//this is the string that contains the decompressed XML server response
String globalUncompressedDataStr = "";

byte gzippedResponse[RESPONSE_BUFFER_SIZE];
size_t responseIndex = 0;

// Choose the analog pin to get semi-random data from for SSL
const int rand_pin = 26;

// Initialize the SSL client library
// We input an EthernetClient, our trust anchors, and the analog pin
EthernetClient base_client;
SSLClient client(base_client, TAs, (size_t)TAs_NUM, rand_pin);

// Variables to measure the speed
unsigned long beginMicros, endMicros;
unsigned long byteCount = 0;
bool printWebData = true;  

void decompressGzippedData(const uint8_t *gzippedData, size_t gzippedDataSize);
uint32_t getUncompressedLength(const uint8_t* data, size_t dataSize);

void setup() {

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

  Ethernet.init(17);  
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  
  /*
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  */
  
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
  } else {
    Serial.print("DHCP assigned IP ");
    Serial.println(Ethernet.localIP());
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);

  Serial.print("connecting to ");
  Serial.print(server);
  Serial.println("...");

  // if you get a connection, report back via serial:
  auto start = millis();
  // specify the server and port, 443 is the standard port for HTTPS
  if (client.connect(server, 443)) {
    auto time = millis() - start;
    Serial.print("Took: ");
    Serial.println(time);
    // Make a HTTP request:
    String getLine = ("GET /transit/StopMonitoring?api_key=" + APIkey + "&agency=SF&stopCode=" + stopCodes[0] + "&format=xml HTTP/1.1");
    client.println(getLine);
    client.println("User-Agent: RP2040 / W5500-EVB-Pico SSLClientOverEthernet");
    client.print("Host: ");
    client.println(server_host);
    client.println("Connection: close");
    client.println();
  } else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }
  beginMicros = micros();
}
  


void loop() {
  
  // Assuming 'client' is already connected and has made a request.
  String line;
  bool isGzip = false;
  bool headersFinished = false; // Flag to indicate when headers are completely read.

  // Read and discard headers
  while (client.available()) {
    line = client.readStringUntil('\n');
    Serial.println(line);

    // Check for gzip encoding in headers
    if (line.startsWith("Content-Encoding: gzip")) {
      isGzip = true;
    }

    // Check if we've reached the end of the headers
    if (line == "\r") {
      headersFinished = true;
      break; // Exit the loop as headers are done.
    }
  }
    
  if (isGzip) {
    // New loop to read data until connection is closed
    while (client.connected() || client.available()) {
      while (client.available()) {
        byte buffer[2048]; // Adjust size as needed
        int bytesRead = client.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
          // Ensure we do not overflow the gzippedResponse buffer
          size_t toCopy = min(bytesRead, RESPONSE_BUFFER_SIZE - responseIndex);
          memcpy(gzippedResponse + responseIndex, buffer, toCopy);
          responseIndex += toCopy;
          byteCount += bytesRead;
        }
      }
    }
  }
  
  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    endMicros = micros();
    if (responseIndex > 0) {
      decompressGzippedData(gzippedResponse, responseIndex);
    }

    Serial.println("disconnecting.");
    client.stop();
    Serial.print("Received ");
    Serial.print(byteCount);
    Serial.print(" bytes decompressed to ");
    Serial.print(globalUncompressedDataStr.length());
    Serial.println();

    // do nothing forevermore:
    while (true) {
      delay(1);
    }
  } 
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
        Serial.println("\nUncompressed Data Size: " + String(globalUncompressedDataStr.length()));
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