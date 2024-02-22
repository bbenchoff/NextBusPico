#ifndef Globals_h
#define Globals_h

#define RESPONSE_BUFFER_SIZE 2048 // Adjust based on your expected response size
#define ETHERNET_LARGE_BUFFERS

//// Data for output. This is the data structure we actually use
#define MAX_ARRIVALS 100 // Maximum number of arrivals we can store

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
int arrivalCount = 0; // Keeps track of the current number of arrivals stored

//// Network Stuff
// This uses ArduinoUniqueID to pull the serial number off of the Flash chip.
// Then, the least significant 6 bytes of the serial are used for the MAC.
byte mac[] = {};
String uniqueIDString = "";
String MACAddress = "";
// user agent; change this if I ever move this to different hardware.
String User_Agent = "RP2040 / W5500-EVB-Pico SSLClientOverEthernet";
// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 177);
IPAddress myDns(8, 8, 8, 8);
// Choose the analog pin to get semi-random data from for SSL
const int rand_pin = 26;
const char server[] = "api.511.org";
const char server_host[] = "api.511.org";
// Initialize the SSL client library
// We input an EthernetClient, our trust anchors, and the analog pin
EthernetClient base_client;
SSLClient client(base_client, TAs, (size_t)TAs_NUM, rand_pin);

//// The response stuff; Gzip and JSON data
int currentStopCodeIndex = 0; // Global index to track the current stop code being fetched
StopCodeData stopCodeDataArray[sizeof(stopCodes)/sizeof(stopCodes[0])];
byte gzippedResponse[RESPONSE_BUFFER_SIZE];
size_t responseIndex = 0;
//this is the string that contains the decompressed XML server response
String globalUncompressedDataStr = "";

//// Time Stuff
// Variables to measure the speed and timing
unsigned long beginMicros, endMicros;
unsigned long byteCount = 0;
time_t currentTime;
unsigned long previousMillis = 0; // Stores the last time the update was executed
const long interval = 65000; // Interval at which to run (65 seconds)

//// E-paper stuff
unsigned char image[700];
Paint paint(image, 0, 0);    // width should be the multiple of 8 


#endif