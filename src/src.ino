#include "arduino_secrets.h"
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Arduino_MKRENV.h>

const int JSON_SIZE = 1000;
const int HEATER_PIN = 0;
const int MIC_PIN = A0;
const int BUZZER_PIN = A1;
const int WATER_LEVEL_PIN = A2;

// Add WiFi connection information
char ssid[] = SECRET_SSID;     //  your network SSID (name)
char pass[] = SECRET_PASS;  // your network password

int WiFistatus = WL_IDLE_STATUS;     // the Wifi radio's status



// MQTT objects
void callback(char* topic, byte* payload, unsigned int length);
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, callback, wifiClient);

// variables to hold data
StaticJsonDocument<JSON_SIZE> jsonDoc;
JsonObject payload = jsonDoc.to<JsonObject>();
JsonObject status = payload.createNestedObject("d");
static char msg[JSON_SIZE];

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");

  payload[length] = 0; // ensure valid content is zero terminated so can treat as c-string
  Serial.println((char *)payload);
}




void setup() {
  pinMode(HEATER_PIN,OUTPUT);
  pinMode(MIC_PIN,INPUT);
  pinMode(BUZZER_PIN,OUTPUT);
  
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  if (!ENV.begin()) { //check correct initialisation of shield
    Serial.println("Failed to initialize MKR ENV shield!");
    while (1); //infinite loop, avoids loop to be executed
  }
  // attempt to connect to Wifi network:

  while (WiFistatus != WL_CONNECTED) {
    Serial.print("Attempting to connect to network: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    WiFistatus = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }

  // you're connected now, so print out the data:
  Serial.println("You're connected to the network");
  
  Serial.println("----------------------------------------");
  printData();
  Serial.println("----------------------------------------");

  // Connect to MQTT - IBM Watson IoT Platform
  if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
    Serial.println("MQTT Connected");
    mqtt.subscribe(MQTT_TOPIC_DISPLAY);

  } else {
    Serial.println("MQTT Failed to connect!");
  }
}

void loop() {
  mqtt.loop();
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
      Serial.println("MQTT Connected");
      mqtt.subscribe(MQTT_TOPIC_DISPLAY);
      mqtt.loop();
    } else {
      Serial.println("MQTT Failed to connect!");
      delay(5000);
    }
  }
  float temp = ENV.readTemperature();
  float humi = ENV.readHumidity();
  float illu = ENV.readIlluminance();
  int sound = analogRead(MIC_PIN);
  float water = analogRead(WATER_LEVEL_PIN);

  // Check if any reads failed and exit early (to try again).
  if (isnan(temp) || isnan(humi) || isnan(illu)) {
    Serial.println("Failed to read data!");
  } else {

    // Send data to Watson IoT Platform
    status["temp"] = temp;
    status["humi"] = humi;
    status["illu"] = illu;
    status["sound"] = sound;
    status["water"] = water;


    serializeJson(jsonDoc, msg, JSON_SIZE);

    Serial.println(msg);
    if (!mqtt.publish(MQTT_TOPIC, msg)) {
      Serial.println("MQTT Publish failed");
    }
  }

  // Pause - but keep polling MQTT for incoming messages
  for (int i = 0; i < 4; i++) {
    mqtt.loop();
    delay(1000);
  }
}

void printData() {
  Serial.println("Board Information:");
  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  Serial.println();
  Serial.println("Network Information:");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}
