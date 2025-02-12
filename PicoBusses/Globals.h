#ifndef Globals_h
#define Globals_h

#define MAX_ARRIVALS 20 // Maximum number of arrivals we can store

struct BusArrival {
    String lineRef;
    String expectedArrivalTimeStr;
    String destinationDisplay;
    String stopPointName;
    time_t expectedArrivalEpoch;
};

struct StopCodeData {
    String stopCode;
    BusArrival arrivals[MAX_ARRIVALS];
    int arrivalCount = 0;
};

BusArrival arrivals[MAX_ARRIVALS];
int arrivalCount = 0;

// Network Stuff
String User_Agent = "Bus Display";
const char server[] = "api.511.org";
const char server_host[] = "api.511.org";

WiFiClient wifiClient;
SSLClient client(wifiClient, TAs, (size_t)TAs_NUM, -1, 1);

// Data tracking
int currentStopCodeIndex = 0;
StopCodeData stopCodeDataArray[sizeof(stopCodes)/sizeof(stopCodes[0])];
String globalUncompressedDataStr = "";

// Timing variables
unsigned long beginMicros, endMicros;
unsigned long byteCount = 0;
time_t currentTime;
unsigned long previousMillis = 0;
const long interval = 65000;


#endif