#include "setup.h"

void printData() {
  Serial.println("----------------------------------------");
  Serial.println("Board Information:");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println();
  Serial.println("Network Information:");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("signal strength (RSSI):");
  Serial.println(WiFi.RSSI());

  Serial.print("Encryption Type:");
  Serial.println(WiFi.encryptionType(), HEX);
  Serial.println();
  Serial.println("----------------------------------------");
}

void initialiseSerial() {
  // Initialize serial and wait for port to open
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect
  }
}

void connectToWiFiNetwork() {
  int WiFistatus = WL_IDLE_STATUS;
  
  while (WiFistatus != WL_CONNECTED) {
    Serial.print("Attempting to connect to network: ");
    Serial.println(SECRET_SSID);
    
    WiFistatus = WiFi.begin(SECRET_SSID, SECRET_PASS);
    delay(10000); // wait 10 seconds for connection
  }

  Serial.println("You're connected to the network");
  printData();
}

void checkENVShieldInitialisation() {
  if (!ENV.begin()) {
    Serial.println("Failed to initialize MKR ENV shield!");
    while (1); // infinite loop, avoids loop to be executed
  }
}
