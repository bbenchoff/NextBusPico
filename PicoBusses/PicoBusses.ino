#include <SPI.h>
#include <EthernetLarge.h>
#include <SSLClient.h>
#include "trust_anchors.h"
#include "miniz.h"

#define RESPONSE_BUFFER_SIZE 4096 // Adjust based on your expected response size
#define ETHERNET_LARGE_BUFFERS

byte gzippedResponse[RESPONSE_BUFFER_SIZE];
size_t responseIndex = 0;

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(54,85,55,79);  // numeric IP for Google (no DNS)
const char server[] = "api.511.org";    // name address for Arduino (using DNS)
const char server_host[] = "api.511.org"; // leave this alone, change only above two

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 177);
IPAddress myDns(8, 8, 8, 8);

// Choose the analog pin to get semi-random data from for SSL
// Pick a pin that's not connected or attached to a randomish voltage source
const int rand_pin = 26;

// Initialize the SSL client library
// We input an EthernetClient, our trust anchors, and the analog pin
EthernetClient base_client;
SSLClient client(base_client, TAs, (size_t)TAs_NUM, rand_pin);
// Variables to measure the speed
unsigned long beginMicros, endMicros;
unsigned long byteCount = 0;
bool printWebData = true;  // set to false for better speed measurement

void decompressGzippedData(const uint8_t *gzippedData, size_t gzippedDataSize);

void setup() {

  Ethernet.init(17);  
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with DHCP:");
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
    Serial.print("  DHCP assigned IP ");
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
    client.println("GET /transit/StopMonitoring?api_key=da03f504-fc16-43e7-a736-319af37570be&agency=SF&stopCode=16633&format=json HTTP/1.1");
    client.println("User-Agent: SSLClientOverEthernet");
    client.println("Accept: application/json");
    //client.println("Accept-Encoding: identity");
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
        byte buffer[16384]; // Adjust size as needed
        int bytesRead = client.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
          // Ensure we do not overflow the gzippedResponse buffer
          size_t toCopy = min(bytesRead, RESPONSE_BUFFER_SIZE - responseIndex);
          memcpy(gzippedResponse + responseIndex, buffer, toCopy);
          responseIndex += toCopy;
          byteCount += bytesRead;
        }
      }
      // Small delay to wait for more data
      //delay(10);
    }
  }
  
  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    endMicros = micros();
    if (responseIndex > 0) {
      decompressGzippedData(gzippedResponse, responseIndex);
    }

    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
    Serial.print("Received ");
    Serial.print(byteCount);
    Serial.print(" bytes in ");
    float seconds = (float)(endMicros - beginMicros) / 1000000.0;
    Serial.print(seconds, 4);
    float rate = (float)byteCount / seconds / 1000.0;
    Serial.print(", rate = ");
    Serial.print(rate);
    Serial.print(" kbytes/second");
    Serial.println();

    // do nothing forevermore:
    while (true) {
      delay(1);
    }
  }
}


void decompressGzippedData(const uint8_t *gzippedData, size_t gzippedDataSize) {
    // Assume a maximum expected uncompressed size
    Serial.print("Attempting to decompress data of size: ");
    Serial.println(gzippedDataSize);

  // Log the first few bytes of the data to confirm it looks like gzip
  Serial.print("Data starts with: ");
  for (int i = 0; i < 800 && i < gzippedDataSize; i++) {
      if (gzippedData[i] < 16) Serial.print("0x0"); // Add leading zero for bytes less than 0x10
      else Serial.print("0x");
      Serial.print(gzippedData[i], HEX);
      if (i < 799 && i < gzippedDataSize - 1) Serial.print(", "); // Add comma and space except after the last item
  }
  Serial.println();

    mz_ulong uncompressedSize = 4096;
    uint8_t *uncompressedData = (uint8_t *)malloc(uncompressedSize);

    if (uncompressedData == NULL) {
        Serial.println("Failed to allocate memory for decompression.");
        return;
    }

    int status = mz_uncompress(uncompressedData, &uncompressedSize, gzippedData, gzippedDataSize);
    if (status == MZ_OK) {
        Serial.println("Decompression successful.");
        Serial.write(uncompressedData, uncompressedSize);
    } else {
        Serial.print("Decompression failed: ");
        Serial.println(status);
    }

    free(uncompressedData);
}
