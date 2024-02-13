/* This is a list of stop codes to gather. These are 
// Unique to _you_, and this should be the only thing
// you change. For example, if you want this device to 
// report on the stops at 9th Ave & Irving St (#17999),
// Lincoln Way & 9th Ave on bus 7 (#15301), and 9th &
// Lincoln Way for the 44 (#13220), this line would be:
//
//  String stopCodes[] = {"19777", "15301", "13220"};
//
// You cna find the stop codes for your bus stop by
// looking at goole maps and clicking on the bus stop
// you want to access with this device. Or look at your
// transit agency's website or something.
*/
String stopCodes[] = {"15696", "15565", "13220", "15678"};
//String stopCodes[] = {"15678"};

// This is your API key. You need a unique one
// Sign up at https://511.org/open-data/token
String APIkey = "da03f504-fc16-43e7-a736-319af37570be";

// You should not have to adjust anything below this line.

#include "Globals.h"
#include "miniz.h"
#include "trust_anchors.h"

#include <ArduinoJson.h>
#include <ArduinoUniqueID.h>
#include <EthernetLarge.h>
#include <NTPClient.h>
#include <SPI.h>
#include <SSLClient.h>
#include <TimeLib.h>

void generateMAC(void);

String CurrentTimeToString(time_t time);
time_t iso8601ToEpoch(String datetime);

void getData(void);
void decompressGzippedData(const uint8_t *gzippedData, size_t gzippedDataSize);
uint32_t getUncompressedLength(const uint8_t* data, size_t dataSize);
String extractExpectedArrivalTime(String data, time_t currentTime);
void parseAndFormatBusArrivals(const String& jsonData);
void removeOldArrivals(void);
void displayArrivals(void);


void setup() {

  generateMAC();
  Ethernet.init(17); // 17 is specific to the W5500-EVB
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  delay(2000); //a non-blocking way for the Serial to connect?

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
  
  delay(1000); // give the Ethernet shield a second to initialize:

  for (int i = 0; i < sizeof(stopCodes)/sizeof(stopCodes[0]); i++) {
    stopCodeDataArray[i].stopCode = stopCodes[i];
    stopCodeDataArray[i].arrivalCount = 0; // Initialize the count of arrivals to 0
  }

  currentTime = now();

}
  
void loop() {

  unsigned long currentMillis;
  currentMillis = millis(); // capture the current time

  if (currentMillis - previousMillis >= interval) { // check if 65 seconds have passed
    previousMillis = currentMillis;
    
    getData(stopCodeDataArray[currentStopCodeIndex].stopCode);
    Serial.println(CurrentTimeToString(currentTime));
    removeOldArrivals();

    if (globalUncompressedDataStr.length() > 0) {
        Serial.print("JSON string length: ");
        Serial.println(globalUncompressedDataStr.length());
        parseAndFormatBusArrivals(globalUncompressedDataStr);
        displayArrivals();
        currentStopCodeIndex = (currentStopCodeIndex + 1) % (sizeof(stopCodes)/sizeof(stopCodes[0]));
    } else {
        Serial.println("No data or failed to fetch data");
    }
    Serial.println("");
    Serial.println("");
  }
}

void generateMAC(){
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

String CurrentTimeToString(time_t time) {
    char timeString[20];

    snprintf(timeString, sizeof(timeString), "%04d-%02d-%02d %02d:%02d:%02d", 
             year(time), month(time), day(time), hour(time), minute(time));

    return String(timeString);
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

    return makeTime(tm);
}

void getData(String stopCode){
  const int maxRetries = 3; // Maximum number of connection retries
  int retryCount = 0;
  bool connected = false;
  bool isGzip = false;
  String line;

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

  client.stop();

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

  // This subtracts 8 bytes from the header, because that's how this works
  const size_t gzipHeaderSize = 10;
  const uint8_t *deflateData = gzippedData + gzipHeaderSize;
  size_t deflateDataSize = gzippedDataSize - gzipHeaderSize - 8; // Subtract 8 bytes for the gzip footer

  mz_ulong outBytes = expectedUncompressedSize;
  int status = tinfl_decompress_mem_to_mem(uncompressedData, outBytes, deflateData, deflateDataSize, 0);

  if (status == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
      Serial.println("Decompression failed.");
  } 
  else {
    Serial.println("Decompression successful.");
    globalUncompressedDataStr = "";

    // Convert uncompressed data to String
    for (size_t i = 0; i < outBytes; ++i) {
      globalUncompressedDataStr += (char)uncompressedData[i];
    }

    globalUncompressedDataStr.trim(); // Removes whitespace from the beginning and end
    Serial.println(globalUncompressedDataStr);
  }
  free(uncompressedData);
}

uint32_t getUncompressedLength(const uint8_t* data, size_t dataSize) {
    if (dataSize < 4) return 0; // this isn't right, bail.

    // The uncompressed size is stored in the last 4 bytes
    // of the gzip stream in little-endian format
    uint32_t uncompressedSize = data[dataSize - 4]
        | (data[dataSize - 3] << 8)
        | (data[dataSize - 2] << 16)
        | (data[dataSize - 1] << 24);

    return uncompressedSize;
}

//This might be a vestigial function, I have no idea delete this maybe?
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

void parseAndFormatBusArrivals(const String& jsonData) {
  // Clear existing data for the current stop code
  stopCodeDataArray[currentStopCodeIndex].arrivalCount = 0;

  // The first characters I'm getting are 0xEF 0xBB 0xBF, a byte order mark
  //I need to delete those before deserializing.
  String cleanJsonData = jsonData.substring(3); 

  DynamicJsonDocument doc(40000);

  DeserializationError error = deserializeJson(doc, cleanJsonData);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  // Extract the ResponseTimestamp from the JSON data
  const char* responseTimestamp = doc["ServiceDelivery"]["ResponseTimestamp"];
  if (responseTimestamp) {
      // Convert ISO8601 to epoch
      time_t serverTime = iso8601ToEpoch(String(responseTimestamp));

      Serial.print(F("Server Time (epoch): "));
      Serial.println(serverTime);

      setTime(serverTime);
      currentTime = serverTime;
  }

  JsonArray arr = doc["ServiceDelivery"]["StopMonitoringDelivery"]["MonitoredStopVisit"];
    int count = 0;
    for(JsonObject obj : arr) {
        if (count >= MAX_ARRIVALS) break; // Prevent overflow of arrivals array

        String lineRef = obj["MonitoredVehicleJourney"]["LineRef"].as<String>();
        String expectedArrivalTimeStr = obj["MonitoredVehicleJourney"]["MonitoredCall"]["ExpectedArrivalTime"].as<String>();
        String destinationDisplay = obj["MonitoredVehicleJourney"]["MonitoredCall"]["DestinationDisplay"].as<String>();
        String stopPointName = obj["MonitoredVehicleJourney"]["MonitoredCall"]["StopPointName"].as<String>();

        // Store the arrival data
        BusArrival& arrival = stopCodeDataArray[currentStopCodeIndex].arrivals[count];
        arrival.lineRef = lineRef;
        arrival.expectedArrivalTimeStr = expectedArrivalTimeStr;
        arrival.destinationDisplay = destinationDisplay;
        arrival.stopPointName = stopPointName;

        count++;
    }
    // Update the count of arrivals for the current stop code
    stopCodeDataArray[currentStopCodeIndex].arrivalCount = count;
}

void removeOldArrivals() {
    int i = 0;
    while (i < arrivalCount) {
        time_t expectedArrivalEpoch = iso8601ToEpoch(arrivals[i].expectedArrivalTimeStr);
        if (expectedArrivalEpoch <= now()) {
            // Shift all subsequent arrivals one position to the left
            for (int j = i; j < arrivalCount - 1; j++) {
                arrivals[j] = arrivals[j + 1];
            }
            arrivalCount--; // Decrease the count of arrivals
        } else {
            i++; // Only increase i if we didn't remove an arrival
        }
    }
}

void displayArrivals() {
  Serial.println();
  currentTime = now(); // Ensure we have the current time updated

  for (int stopCodeIndex = 0; stopCodeIndex < sizeof(stopCodes) / sizeof(stopCodes[0]); stopCodeIndex++) {
    String lineRefs[100];
    String destinations[100];
    String stopPoints[100];
    String minutesLists[100];
    int uniqueLines = 0;

    for (int arrivalIndex = 0; arrivalIndex < stopCodeDataArray[stopCodeIndex].arrivalCount; arrivalIndex++) {
      BusArrival& arrival = stopCodeDataArray[stopCodeIndex].arrivals[arrivalIndex];
      time_t expectedArrivalEpoch = iso8601ToEpoch(arrival.expectedArrivalTimeStr);
      long minutesUntilArrival = (expectedArrivalEpoch - currentTime) / 60;

      if (expectedArrivalEpoch <= currentTime) continue; // Skip past or invalid arrivals

      // Find if this lineRef is already noted, else add it
      int lineIndex = -1;
      for (int i = 0; i < uniqueLines; i++) {
        if (lineRefs[i] == arrival.lineRef && destinations[i] == arrival.destinationDisplay && stopPoints[i] == arrival.stopPointName) {
          lineIndex = i;
          break;
        }
      }

      if (lineIndex == -1) { // New lineRef
        lineIndex = uniqueLines++;
        lineRefs[lineIndex] = arrival.lineRef;
        destinations[lineIndex] = arrival.destinationDisplay;
        stopPoints[lineIndex] = arrival.stopPointName;
        minutesLists[lineIndex] = String(minutesUntilArrival);
      } 
      else { // Existing lineRef, append minutes
        minutesLists[lineIndex] += ", " + String(minutesUntilArrival);
      }
    }

    // Now print out the aggregated information
    for (int i = 0; i < uniqueLines; i++) {
      Serial.print(lineRefs[i] + " to ");
      Serial.print(destinations[i] + " in ");
      Serial.print(minutesLists[i] + " minutes at ");
      Serial.println(stopPoints[i]);
    }
  }
  Serial.println(); // Extra line for readability
}

