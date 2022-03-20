#include <WiFiNINA.h>
#include <Arduino_MKRENV.h>

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
