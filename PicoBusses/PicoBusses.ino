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
#include "transit_bitmap.h"
#include "muni_bitmap.h"
#include "Fonts/FreeSansBold18pt7b.h"
#include "Fonts/FreeSansBold24pt7b.h"
#include "Fonts/FreeSans12pt7b.h"
#include <Adafruit_GFX.h>

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
//const char* password = "666HailSatanWRONG!";
const char* password = "666HailSatan!";

struct LineInfo {
  String lineRef;         // The line number/reference
  String destination;     // The destination 
  String stopPoint;       // The stop point
  bool active;            // Whether this line is active in current update
};

String CurrentTimeToString(time_t time);
time_t iso8601ToEpoch(String datetime);
MT_EPD display(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);

bool positionsInitialized = false;

LineInfo lineInfoArray[10];
int lineInfoCount = 0;

const int ROW_HEIGHT = 135;


void printHexBuffer(const uint8_t* buffer, size_t length);
void getData(void);
void decompressGzippedData(const uint8_t *gzippedData, size_t gzippedDataSize);
uint32_t getUncompressedLength(const uint8_t* data, size_t dataSize);
void parseAndFormatBusArrivals(const String& jsonData);
void removeOldArrivals(void);
void displayArrivals(void);
void resetDevice(void);
bool connectToWiFiWithTimeout(void);
void collectActiveLines(void);
void updateDisplay(void);
const uint8_t* getTransitLogo(String lineRef);


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("EPD image test");
  
  // Configure pins
  pinMode(EPD_BS, OUTPUT);
  pinMode(EPD_CS, OUTPUT);
  pinMode(EPD_DC, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  pinMode(EPD_BUSY, INPUT);
  
  digitalWrite(EPD_BS, LOW);
  digitalWrite(EPD_CS, HIGH);
  digitalWrite(EPD_DC, HIGH);
  digitalWrite(EPD_RST, HIGH);
  
  SPI.setSCK(EPD_SCK);
  SPI.setTX(EPD_MOSI);
  SPI.begin();
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  
  // Initialize display
  display.begin();
  display.setRotation(1);
  display.clearDisplay();
  
  // Draw the bitmap at position (0,0)
  /* Keeping this here for later, these are the 'rows'
  display.drawBitmap(0, 0, transit_logo_48, 130, 130, MT_EPD::EPD_BLACK);
  display.drawBitmap(0, 135, transit_logo_L, 130, 130, MT_EPD::EPD_BLACK);
  display.drawBitmap(0, 270, transit_logo_LOWL, 130, 130, MT_EPD::EPD_BLACK);
  display.drawBitmap(0, 405, transit_logo_CA, 130, 130, MT_EPD::EPD_BLACK);
  display.drawBitmap(0, 540, transit_logo_714, 130, 130, MT_EPD::EPD_BLACK);
  display.drawBitmap(0, 675, transit_logo_39T, 130, 130, MT_EPD::EPD_BLACK);
  

  display.drawBitmap(0, 0, transit_logo_48, 130, 130, MT_EPD::EPD_BLACK);

  display.setFont(&FreeSans12pt7b);
  display.setCursor(135, 25);
  display.setTextColor(MT_EPD::EPD_BLACK);
  display.println("To 22nd St + Iowa St");

  display.setFont(&FreeSans12pt7b);
  display.setCursor(135, 60);
  display.setTextColor(MT_EPD::EPD_BLACK);
  display.println("From 39th Ave & Rivera St");

  // Test the FreeSansBold24pt7b font
  display.setFont(&FreeSansBold24pt7b);
  display.setCursor(150, 115);
  display.setTextColor(MT_EPD::EPD_BLACK);
  display.println("20, 120, 180");
  */

  display.drawBitmap(0, 250, epd_bitmap_Muni_worm_logo, 480, 258, MT_EPD::EPD_RED);

  
  // Do ONE full update with both logo and initial text
  display.display();
  // This is important - must fully complete before continuing
  delay(10000); // Give extra time to ensure display update is complete
  
  // Connect to WiFi with timeout
  if (!connectToWiFiWithTimeout()) {
    resetDevice(); // Loop back to the beginning, we're fucked.
  }

  // Figure out the stop codes for all this shit
  for (int i = 0; i < sizeof(stopCodes)/sizeof(stopCodes[0]); i++) {
    stopCodeDataArray[i].stopCode = stopCodes[i];
    stopCodeDataArray[i].arrivalCount = 0; // Initialize the count of arrivals to 0
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
        updateDisplay();
    } else {
        Serial.println("No data or failed to fetch data");
    }
    Serial.println("");
    Serial.println("");
  }
}

void resetDevice() {
  // On occasion of the WiFi not working, need to tell the user.
  display.begin();
  display.setCursor(20, 490);
  display.setTextSize(2);
  display.setTextColor(0x0000);
  display.println("WiFi Not Connected!");
  display.setCursor(20, 510);
  display.println("Check WiFi credentials");
  display.setCursor(20, 530);
  display.println("Resetting...");
  display.display();


  // Give the user time to read the message
  delay(60000);
  
  // Reset the Pico
  // Method 1: Use watchdog if available
  #if defined(ARDUINO_ARCH_RP2040)
    // Import at the top of your file: #include <pico/stdlib.h>
    watchdog_enable(1, 1);  // Enable watchdog with 1ms timeout
    while(1);  // Wait for watchdog to reset
  #else
    // Method 2: Software reset for Pico using Arduino
    NVIC_SystemReset();  // System reset
  #endif
}

bool connectToWiFiWithTimeout() {
  Serial.print("Connecting to WiFi");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Try to connect for 3 minutes (180 seconds)
  const unsigned long timeout = 180000; // 3 minutes in milliseconds
  unsigned long startTime = millis();
  
  while (WiFi.status() != WL_CONNECTED) {
    // Check if timed out
    if (millis() - startTime > timeout) {
      Serial.println("\nWiFi connection timed out after 3 minutes!");
      return false;
    }
    
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  return true;
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



void collectActiveLines() {
  // Mark all lines as inactive initially
  for (int i = 0; i < lineInfoCount; i++) {
    lineInfoArray[i].active = false;
  }
  
  // Collect all current active lines from arrivals data
  for (int stopCodeIndex = 0; stopCodeIndex < sizeof(stopCodes) / sizeof(stopCodes[0]); stopCodeIndex++) {
    for (int arrivalIndex = 0; arrivalIndex < stopCodeDataArray[stopCodeIndex].arrivalCount; arrivalIndex++) {
      BusArrival& arrival = stopCodeDataArray[stopCodeIndex].arrivals[arrivalIndex];
      
      // Check if arrival is in the future
      time_t expectedArrivalEpoch = iso8601ToEpoch(arrival.expectedArrivalTimeStr);
      if (expectedArrivalEpoch <= currentTime) continue;
      
      // Create a unique key for this line+destination+stop combination
      String lineKey = arrival.lineRef + "|" + arrival.destinationDisplay + "|" + arrival.stopPointName;
      
      // Check if we already have this line in our array
      bool lineFound = false;
      for (int i = 0; i < lineInfoCount; i++) {
        String existingKey = lineInfoArray[i].lineRef + "|" + lineInfoArray[i].destination + "|" + lineInfoArray[i].stopPoint;
        if (existingKey == lineKey) {
          // Found existing entry, mark as active
          lineInfoArray[i].active = true;
          lineFound = true;
          break;
        }
      }
      
      // If not found and we have space, add it
      if (!lineFound && lineInfoCount < 10) {
        lineInfoArray[lineInfoCount].lineRef = arrival.lineRef;
        lineInfoArray[lineInfoCount].destination = arrival.destinationDisplay;
        lineInfoArray[lineInfoCount].stopPoint = arrival.stopPointName;
        lineInfoArray[lineInfoCount].active = true;
        lineInfoCount++;
      }
    }
  }
}

void updateDisplay() {
  // First collect all active lines
  collectActiveLines();
  
  // Clear the display
  display.clearDisplay();
  
  // Count how many active lines we have
  int activeCount = 0;
  for (int i = 0; i < lineInfoCount; i++) {
    if (lineInfoArray[i].active) {
      activeCount++;
    }
  }
  
  // Calculate how many lines we can display (max 6 due to screen height)
  int displayCount = min(activeCount, 6);
  
  // Display active lines (they will naturally fill from top to bottom)
  int displayedLines = 0;
  
  for (int i = 0; i < lineInfoCount && displayedLines < displayCount; i++) {
    // Skip inactive lines
    if (!lineInfoArray[i].active) continue;
    
    // Calculate vertical position (lines are stacked from top down)
    int displayY = displayedLines * ROW_HEIGHT;
    String lineRef = lineInfoArray[i].lineRef;
    String destination = lineInfoArray[i].destination;
    String stopPoint = lineInfoArray[i].stopPoint;
    
    // Collect arrival times for this specific line+destination+stop
    String arrivalTimes = "";
    
    // Find all arrival times for this specific line+destination+stop
    for (int stopCodeIndex = 0; stopCodeIndex < sizeof(stopCodes) / sizeof(stopCodes[0]); stopCodeIndex++) {
      for (int arrivalIndex = 0; arrivalIndex < stopCodeDataArray[stopCodeIndex].arrivalCount; arrivalIndex++) {
        BusArrival& arrival = stopCodeDataArray[stopCodeIndex].arrivals[arrivalIndex];
        
        // Check if this matches our line info
        if (arrival.lineRef != lineRef || 
            arrival.destinationDisplay != destination || 
            arrival.stopPointName != stopPoint) {
          continue;
        }
        
        // Check if arrival is in the future
        time_t expectedArrivalEpoch = iso8601ToEpoch(arrival.expectedArrivalTimeStr);
        if (expectedArrivalEpoch <= currentTime) continue;
        
        // Calculate minutes until arrival
        long minutesUntilArrival = (expectedArrivalEpoch - currentTime) / 60;
        
        // Add to arrival times list
        if (arrivalTimes.length() > 0) {
          arrivalTimes += ", ";
        }
        arrivalTimes += String(minutesUntilArrival);
      }
    }
    
    // Only display if we have arrival times
    if (arrivalTimes.length() > 0) {
      // Draw the line's logo
      display.drawBitmap(0, displayY, getTransitLogo(lineRef), 130, 130, MT_EPD::EPD_BLACK);
      
      // Draw destination info with FreeSans12pt7b font
      display.setFont(&FreeSans12pt7b);
      display.setCursor(135, displayY + 25);
      display.setTextColor(MT_EPD::EPD_BLACK);
      display.println("To " + destination);
      
      // Draw stop info
      display.setCursor(135, displayY + 60);
      display.println("From " + stopPoint);
      
      // Draw arrival times in larger FreeSansBold24pt7b font
      display.setFont(&FreeSansBold24pt7b);
      display.setCursor(150, displayY + 115);
      display.println(arrivalTimes);
      
      // Increment count of displayed lines
      displayedLines++;
    }
  }
  
  // Update the e-paper display
  display.display();
}

const uint8_t* getTransitLogo(String lineRef) {

  /* These are the valid lines
  L 30X FBUS 29 19 1X 23 24 25 27 714 90 28 14R 18 14 2 21
  22 33 12 36 38R 5 30 44 45 58 35 37 38 31 48 39 49 43 55
  54 1 57 56 5R 6 8 8BX KBUS F 9R 7 M 66 CA J K N 67 8AX
  NBUS 15 91 LOWL 9 NOWL T TBUS 52 PH PM 28R
  */

  // Map line references to appropriate bitmap
  if (lineRef == "L") return transit_logo_L;
  else if (lineRef == "30X") return transit_logo_30;
  else if (lineRef == "FBUS") return transit_logo_F;
  else if (lineRef == "29") return transit_logo_29;
  else if (lineRef == "19") return transit_logo_19;
  else if (lineRef == "1X") return transit_logo_1X;
  else if (lineRef == "23") return transit_logo_23;
  else if (lineRef == "24") return transit_logo_24;
  else if (lineRef == "25") return transit_logo_25;
  else if (lineRef == "27") return transit_logo_27;
  else if (lineRef == "714") return transit_logo_714;
  else if (lineRef == "90") return transit_logo_90;
  else if (lineRef == "28") return transit_logo_28;
  else if (lineRef == "14R") return transit_logo_14R;
  else if (lineRef == "18") return transit_logo_18;
  else if (lineRef == "14") return transit_logo_14;
  else if (lineRef == "2") return transit_logo_2;
  else if (lineRef == "21") return transit_logo_21;
  else if (lineRef == "22") return transit_logo_22;
  else if (lineRef == "33") return transit_logo_33;
  else if (lineRef == "12") return transit_logo_12;
  else if (lineRef == "36") return transit_logo_36;
  else if (lineRef == "38R") return transit_logo_38R;
  else if (lineRef == "5") return transit_logo_5;
  else if (lineRef == "30") return transit_logo_30;
  else if (lineRef == "44") return transit_logo_44;
  else if (lineRef == "45") return transit_logo_45;
  else if (lineRef == "58") return transit_logo_58;
  else if (lineRef == "35") return transit_logo_35;
  else if (lineRef == "37") return transit_logo_37;
  else if (lineRef == "38") return transit_logo_38;
  else if (lineRef == "31") return transit_logo_31;
  else if (lineRef == "48") return transit_logo_48;
  else if (lineRef == "39") return transit_logo_39T;
  else if (lineRef == "49") return transit_logo_49;
  else if (lineRef == "43") return transit_logo_43;
  else if (lineRef == "55") return transit_logo_55;
  else if (lineRef == "54") return transit_logo_54;
  else if (lineRef == "1") return transit_logo_1;
  else if (lineRef == "57") return transit_logo_57;
  else if (lineRef == "56") return transit_logo_56;
  else if (lineRef == "5R") return transit_logo_5R;
  else if (lineRef == "6") return transit_logo_6;
  else if (lineRef == "8") return transit_logo_8;
  else if (lineRef == "8BX") return transit_logo_8BX;
  else if (lineRef == "KBUS") return transit_logo_KBUS;
  else if (lineRef == "F") return transit_logo_F;
  else if (lineRef == "9R") return transit_logo_9R;
  else if (lineRef == "7") return transit_logo_7;
  else if (lineRef == "M") return transit_logo_M;
  else if (lineRef == "66") return transit_logo_66;
  else if (lineRef == "CA") return transit_logo_CA;
  else if (lineRef == "J") return transit_logo_J;
  else if (lineRef == "K") return transit_logo_K;
  else if (lineRef == "N") return transit_logo_N;
  else if (lineRef == "67") return transit_logo_67;
  else if (lineRef == "8AX") return transit_logo_8AX;
  else if (lineRef == "NBUS") return transit_logo_NBUS;
  else if (lineRef == "15") return transit_logo_15;
  else if (lineRef == "91") return transit_logo_91;
  else if (lineRef == "LOWL") return transit_logo_LOWL;
  else if (lineRef == "9") return transit_logo_9;
  else if (lineRef == "NOWL") return transit_logo_NOWL;
  else if (lineRef == "T") return transit_logo_T;
  else if (lineRef == "TBUS") return transit_logo_TBUS;
  else if (lineRef == "52") return transit_logo_52;
  else if (lineRef == "PH") return transit_logo_PH;
  else if (lineRef == "PM") return transit_logo_PM;
  else if (lineRef == "28R") return transit_logo_28R;
  
  // Default logo for unknown lines
  return transit_logo_39T;  // Use a default logo/ the weird coit one
}
