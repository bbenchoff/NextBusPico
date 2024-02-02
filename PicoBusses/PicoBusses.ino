/****************************************************************************************************************************
  WebClient.ino - Simple Arduino web server sample for Ethernet shield

  Ethernet_Generic is a library for the W5x00 Ethernet shields trying to merge the good features of
  previous Ethernet libraries

  Built by Khoi Hoang https://github.com/khoih-prog/Ethernet_Generic
 *****************************************************************************************************************************/

#include "defines.h"
#include <SSLClient.h>
#include "trust_anchors.h"
#include <ArduinoUniqueID.h>

char server[] = "arduino.cc";
byte macAddress[] = {};
String uniqueIDString = "";
String MACAddress = "";

// Initialize the Ethernet client object
EthernetClient baseclient;
SSLClient client(baseclient, TAs, (size_t)TAs_NUM, 1);

void setup()
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
      macAddress[i] = hexByte;
    }
  }

  // Finally, set the String MACAddress to the mac I just generated. Yes,
  // we're doing it this way.
  for (int i = 0; i < 6; i++) {
    // Convert the byte to a hexadecimal string
    if (macAddress[i] < 16) {
        // Adding leading zero for bytes less than 16 (0x10)
        MACAddress += "0";
    }
    MACAddress += String(macAddress[i], HEX);
  }
  
  MACAddress.toUpperCase();
  SerialDebug.begin(115200);
  while (!Serial && millis() < 5000);
  delay(500);

  Ethernet.init(17);  // For w5500-evb-pico
  Ethernet.begin(macAddress);

  SerialDebug.print(F("Connected! IP address: "));
  SerialDebug.println(Ethernet.localIP());

  if ( (Ethernet.getChip() == w5500) || (Ethernet.getChip() == w6100) || (Ethernet.getAltChip() == w5100s) )
  {
    if (Ethernet.getChip() == w6100)
      SerialDebug.print(F("W6100 => "));
    else if (Ethernet.getChip() == w5500)
      SerialDebug.print(F("W6100 => "));
    else
      SerialDebug.print(F("W5100S => "));
    
    SerialDebug.print(F("Speed: "));
    SerialDebug.print(Ethernet.speedReport());
    SerialDebug.print(F(", Duplex: "));
    SerialDebug.print(Ethernet.duplexReport());
    SerialDebug.print(F(", Link status: "));
    SerialDebug.println(Ethernet.linkReport());
  }

  SerialDebug.println();
  SerialDebug.println(F("Starting connection to server..."));

  // if you get a connection, report back via serial
  if (client.connect(server, 443))
  {
    SerialDebug.println(F("Connected to server"));
    // Make a HTTP request
    client.println(F("GET /asciilogo.txt HTTP/1.1"));
    client.println(F("Host: arduino.tips"));
    client.println(F("Connection: close"));
    client.println();
  }
}

void printoutData()
{
  // if there are incoming bytes available
  // from the server, read them and print them
  while (client.available())
  {
    char c = client.read();
    SerialDebug.write(c);
    SerialDebug.flush();
  }
}

void loop()
{
  printoutData();

  // if the server's disconnected, stop the client
  if (!client.connected())
  {
    SerialDebug.println();
    SerialDebug.println(F("Disconnecting from server..."));
    client.stop();

    // do nothing forevermore
    while (true)
      yield();
  }
}