/* This is a list of stop codes to gather. These are 
// Unique to _you_, and this should be the only thing
// you change. For example, if you want this device to 
// report on the stops at 9th Ave & Irving St (#17999),
// Lincoln Way & 9th Ave on bus 7 (#15301), and 9th &
// Lincoln Way for the 44 (#13220), this line would be:
//
//  String stopCodes[] = {"19777", "15301", "13220"};
//
// You can find the stop codes for your bus stop by
// looking at goole maps and clicking on the bus stop
// you want to access with this device. Or look at your
// transit agency's website or something.
//
//  Using the actual cable car termini (stops "16063", 
//  "15063", "16644", "13860", and "13889") results in
//  times that are about 58 years in the future. Using
//  the next stop up from the cable car terminus (stops
//  "15081", "16068", "16645", "13855", and "13889")
//  gives you the actual time the next car will arrive.
//  This behavior correctly encodes something unique
//  about San Francisco. Locals do not wait in line at
//  the cable car termini. To board a cable car, you go
//  to the next stop up.
*/
String stopCodes[] = {"13565", "16633"};
//String stopCodes[] = {"15696", "15565", "13220", "15678"};  //Market and Van Ness
//String stopCodes[] = {"69925", "13211", "15300", "13220", "17999", "17996", "15301"}; //9th and Irving
//String stopCodes[] = {"15081", "16068", "16645", "13855", "13889"}; //One block up from each cable car termini
//String stopCodes[] = {"14114", "16061", "14105"}; // Cable Car and Coit demo
//String stopCodes[] = {"13124", "13144"}; // The two busiest stops, 3rd St & Folsom St and 3rd St & Bryant St


// This is your API key. You need a unique one
// Sign up at https://511.org/open-data/token
String APIkey = "da03f504-fc16-43e7-a736-319af37570be";

//#define DEBUG_MODE

// You should not have to adjust anything below this line.

#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include <SSLClient.h>
#include "trust_anchors.h"
#include "miniz.h"
#include <ArduinoUniqueID.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <ArduinoJson.h>
#include "imagedata.h"
#include "Globals.h"

#include "MT_EPD.h"

#define COLORED     0
#define UNCOLORED   1

// Pin definitions
#define EPD_BS    26  // Bus Select
#define EPD_BUSY  22  // Busy signal
#define EPD_RST   21  // Reset
#define EPD_DC    20  // Data/Command
#define EPD_MOSI  19  // SPI MOSI
#define EPD_SCK   18  // SPI Clock
#define EPD_CS    17  // Chip Select

// WiFi credentials
const char* ssid = "LiveLaughLan";
const char* password = "666HailSatan!";

String CurrentTimeToString(time_t time);
time_t iso8601ToEpoch(String datetime);
MT_EPD display(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);

void printHexBuffer(const uint8_t* buffer, size_t length);
void getData(void);
void decompressGzippedData(const uint8_t *gzippedData, size_t gzippedDataSize);
uint32_t getUncompressedLength(const uint8_t* data, size_t dataSize);
void parseAndFormatBusArrivals(const String& jsonData);
void removeOldArrivals(void);
void displayArrivals(void);


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting EPD test");
  
  // Configure pins
  pinMode(EPD_BS, OUTPUT);
  pinMode(EPD_CS, OUTPUT);
  pinMode(EPD_DC, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  pinMode(EPD_BUSY, INPUT);
  
  digitalWrite(EPD_BS, LOW);   // 4-wire mode
  digitalWrite(EPD_CS, HIGH);
  digitalWrite(EPD_DC, HIGH);
  digitalWrite(EPD_RST, HIGH);
  
  SPI.setSCK(EPD_SCK);
  SPI.setTX(EPD_MOSI);
  SPI.begin();
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  
display.begin();
delay(100);

// Calculate buffer size for one-third of the screen
// Display is 800×480 pixels, so each third is 800×160 pixels
// Each byte represents 8 pixels, so we need 800×160/8 = 16000 bytes per section
const int oneThirdBytes = 16000; // One third of the display (in bytes)

// Send data to black/white buffer
display.sendCommand(0x10);

// First third: Black (0x00)
for(int i = 0; i < oneThirdBytes; i++) {
    display.sendData(0x00);  // Black
}

// Second third: White (0xFF)
for(int i = 0; i < oneThirdBytes; i++) {
    display.sendData(0xFF);  // White
}

// Last third: White for red background (0xFF)
for(int i = 0; i < oneThirdBytes; i++) {
    display.sendData(0xFF);  // White (needed for red to show)
}

// Send data to red buffer
display.sendCommand(0x13);

// First third: No red (0x00)
for(int i = 0; i < oneThirdBytes; i++) {
    display.sendData(0x00);  // No red
}

// Second third: No red (0x00)
for(int i = 0; i < oneThirdBytes; i++) {
    display.sendData(0x00);  // No red
}

// Last third: Red (0xFF)
for(int i = 0; i < oneThirdBytes; i++) {
    display.sendData(0xFF);  // Red
}

display.display();
delay(1000);
display.sleep();
    
  //Start the WiFi
  Serial.print("Initializing WiFI\n");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(1000); // give the Ethernet shield a second to initialize:



  for (int i = 0; i < sizeof(stopCodes)/sizeof(stopCodes[0]); i++) {
    stopCodeDataArray[i].stopCode = stopCodes[i];
    stopCodeDataArray[i].arrivalCount = 0; // Initialize the count of arrivals to 0
  }


    Serial.println("Testing connection to Google...");
  if (!wifiClient.connect("172.217.164.110", 443)) {
      Serial.println("Failed to connect to Google! Check DNS or network.");
  } else {
      Serial.println("Connected to Google successfully!");
  }

  Serial.println("finished setup");

}
  
void loop() {

  int counter = 0;
  unsigned long currentMillis;
  currentMillis = millis(); // capture the current time

  if (currentMillis - previousMillis >= interval) { // check if 65 seconds have passed
    previousMillis = currentMillis;
    
    getData(stopCodeDataArray[currentStopCodeIndex].stopCode);
    #ifdef DEBUG_MODE
    Serial.println(CurrentTimeToString(currentTime));
    #endif
    removeOldArrivals();

    if (globalUncompressedDataStr.length() > 0) {
        #ifdef DEBUG_MODE
        Serial.print("JSON string length: ");
        Serial.println(globalUncompressedDataStr.length());
        #endif
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

// Add the implementation of the helper function
void printHexBuffer(const uint8_t* buffer, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (buffer[i] < 0x10) {
      Serial.print("0"); // Add leading zero for values < 0x10
    }
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
    if ((i + 1) % 16 == 0) { // Line break every 16 bytes
      Serial.println();
    }
  }
  Serial.println(); // Final newline
}

void getData(String stopCode) {
  const int maxRetries = 3;
  int retryCount = 0;
  bool requestSuccess = false;

  #ifdef DEBUG_MODE
  Serial.println("Got to getData");
  Serial.println("Attempting to connect to server...");
  #endif

  while (!requestSuccess && retryCount < maxRetries) {
    // Make sure we start with a fresh connection
    client.stop();
    delay(100);  // Give it a moment to clean up

    #ifdef DEBUG_MODE
    Serial.print("Connecting to: ");
    Serial.print(server);
    Serial.println(":443");
    #endif

    if (!client.connect(server, 443)) {
      Serial.println("Connection failed! Check server name, WiFi, or SSL.");
      retryCount++;
      delay(1000);
      continue;
    }

    #ifdef DEBUG_MODE
    Serial.println("Connected to server! Sending request...");
    #endif

    String path = "/transit/StopMonitoring?api_key=" + 
                 APIkey + "&agency=SF&stopCode=" + stopCode + "&format=json";

    // Send HTTP request
    String request = "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + String(server) + "\r\n";
    request += "User-Agent: " + User_Agent + "\r\n";
    request += "Connection: close\r\n\r\n";
    
    // Send the complete request at once
    client.print(request);

    // Wait a bit for the response to start coming in
    delay(100);

    // Read the response while the connection is alive
    String response = "";
    unsigned long timeout = millis();
    
    while (client.connected() && (millis() - timeout < 5000)) {
      while (client.available()) {
        char c = client.read();
        response += c;
        timeout = millis();  // Reset timeout when we receive data
      }
    }

    // Clean up the connection
    client.stop();

    if (response.length() > 0) {
      #ifdef DEBUG_MODE
      Serial.println("\nResponse received:");
      Serial.println(response);
      #endif
      // Look for the end of headers (double newline)
      int bodyStart = response.indexOf("\r\n\r\n");
      if (bodyStart > 0) {
        String body = response.substring(bodyStart + 4);
        #ifdef DEBUG_MODE
        Serial.println("\nBody length: " + String(body.length()));
        #endif
        // Convert to bytes for decompression
        size_t bodyLen = body.length();
        uint8_t* bodyBytes = (uint8_t*)malloc(bodyLen);
        if (bodyBytes != nullptr) {
          memcpy(bodyBytes, body.c_str(), bodyLen);
          #ifdef DEBUG_MODE
          printHexBuffer(bodyBytes, bodyLen);  // Print hex values
          #endif
          decompressGzippedData(bodyBytes, (size_t)bodyLen);  // Explicitly cast to size_t
          free(bodyBytes);
          requestSuccess = true;
        } else {
          #ifdef DEBUG_MODE
          Serial.println("Failed to allocate memory for body");
          #endif
        }
      }
    } else {
      #ifdef DEBUG_MODE
      Serial.println("No response received");
      #endif
      retryCount++;
    }
  }

  if (!requestSuccess) {

    #ifdef DEBUG_MODE
    Serial.println("Failed to get data after all retries");
    #endif
    globalUncompressedDataStr = "";
  }
}

void decompressGzippedData(const uint8_t *gzippedData, size_t gzippedDataSize) {
  #ifdef DEBUG_MODE
  Serial.print("Attempting to decompress data of size: ");
  Serial.println(gzippedDataSize);
  #endif

  // Find the start of the gzip data by looking for the gzip magic numbers
  size_t dataStart = 0;
  for (size_t i = 0; i < gzippedDataSize - 1; i++) {
    if (gzippedData[i] == 0x1F && gzippedData[i + 1] == 0x8B) {
      dataStart = i;
      break;
    }
  }

  // Get the actual gzipped data size
  size_t actualGzipSize = gzippedDataSize - dataStart;
  
  // Use the getUncompressedLength function to determine the expected uncompressed size
  uint32_t expectedUncompressedSize = getUncompressedLength(gzippedData + dataStart, actualGzipSize);
  
  if (expectedUncompressedSize == 0) {
    #ifdef DEBUG_MODE
    Serial.println("Invalid gzip data or couldn't determine uncompressed size");
    #endif
    return;
  }

  uint8_t *uncompressedData = (uint8_t *)malloc(expectedUncompressedSize);
  if (uncompressedData == NULL) {
    #ifdef DEBUG_MODE
    Serial.println("Failed to allocate memory for decompression.");
    #endif
    return;
  }

  // The gzip header is usually 10 bytes
  const size_t gzipHeaderSize = 10;
  const uint8_t *deflateData = gzippedData + dataStart + gzipHeaderSize;
  size_t deflateDataSize = actualGzipSize - gzipHeaderSize - 8; // Subtract 8 bytes for the gzip footer

  mz_ulong outBytes = expectedUncompressedSize;
  int status = tinfl_decompress_mem_to_mem(uncompressedData, outBytes, deflateData, deflateDataSize, 0);

  if (status == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
    #ifdef DEBUG_MODE
    Serial.println("Decompression failed.");
    #endif
  } 
  else {
    #ifdef DEBUG_MODE
    Serial.println("Decompression successful.");
    #endif
    globalUncompressedDataStr = "";

    // Convert uncompressed data to String
    for (size_t i = 0; i < outBytes; ++i) {
      globalUncompressedDataStr += (char)uncompressedData[i];
    }
    #ifdef DEBUG_MODE
    Serial.println("Decompressed data:");
    Serial.println(globalUncompressedDataStr);
    #endif


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

void parseAndFormatBusArrivals(const String& jsonData) {
  // Clear existing data for the current stop code
  stopCodeDataArray[currentStopCodeIndex].arrivalCount = 0;

  // The first characters I'm getting are 0xEF 0xBB 0xBF, a byte order mark.
  //I need to delete those before deserializing.
  String cleanJsonData = jsonData.substring(3); 

  DynamicJsonDocument doc(40000);

  DeserializationError error = deserializeJson(doc, cleanJsonData);

  if (error) {
    #ifdef DEBUG_MODE
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    #endif
    return;
  }

  // Extract the ResponseTimestamp from the JSON data
  const char* responseTimestamp = doc["ServiceDelivery"]["ResponseTimestamp"];
  if (responseTimestamp) {
      // Convert ISO8601 to epoch
      time_t serverTime = iso8601ToEpoch(String(responseTimestamp));

      //Serial.print(F("Server Time (epoch): "));
      //Serial.println(serverTime);

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
            i++; // Only increase if if we didn't remove an arrival
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