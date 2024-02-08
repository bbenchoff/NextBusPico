#ifndef Globals_h
#define Globals_h

#define RESPONSE_BUFFER_SIZE 2048 // Adjust based on your expected response size
#define ETHERNET_LARGE_BUFFERS

// This uses ArduinoUniqueID to pull the serial number off of the Flash chip.
// Then, the least significant 6 bytes of the serial are used for the MAC.
byte mac[] = {};
String uniqueIDString = "";
String MACAddress = "";
String User_Agent = "RP2040 / W5500-EVB-Pico SSLClientOverEthernet";

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 177);
IPAddress myDns(8, 8, 8, 8);

// Choose the analog pin to get semi-random data from for SSL
const int rand_pin = 26;

const char server[] = "api.511.org";
const char server_host[] = "api.511.org";

EthernetUDP Udp;
NTPClient timeClient(Udp);
// Initialize the SSL client library
// We input an EthernetClient, our trust anchors, and the analog pin
EthernetClient base_client;
SSLClient client(base_client, TAs, (size_t)TAs_NUM, rand_pin);

//this is the string that contains the decompressed XML server response
String globalUncompressedDataStr = "";

byte gzippedResponse[RESPONSE_BUFFER_SIZE];
size_t responseIndex = 0;

// Variables to measure the speed and timing
unsigned long beginMicros, endMicros;
unsigned long byteCount = 0;

time_t oneMinute = now();


#endif