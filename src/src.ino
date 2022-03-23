#include "arduino_secrets.h"
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Arduino_MKRENV.h>

const int JSON_SIZE = 1000;

const int HEATER_PIN = 0;
const int WATER_PUMP_PIN = 1;
const int MIC_PIN = A0;
const int BUZZER_PIN = A1;
const int WATER_LEVEL_PIN = A2;
const int MOTOR_PIN = A3;

bool heater_status = false;
bool pump_status = false;

// MQTT objects
void callback(char* topic, byte* payload, unsigned int length);
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, callback, wifiClient);

// variables to hold data
StaticJsonDocument<JSON_SIZE> jsonDoc;
JsonObject payload = jsonDoc.to<JsonObject>();
JsonObject status = payload.createNestedObject("sensor_data");

StaticJsonDocument<JSON_SIZE> jsonDocHeater;
JsonObject payloadHeater = jsonDocHeater.to<JsonObject>();

StaticJsonDocument<JSON_SIZE> jsonDocPump;
JsonObject payloadPump = jsonDocPump.to<JsonObject>();

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
  pinMode(MIC_PIN,INPUT);
  pinMode(WATER_LEVEL_PIN,INPUT);
  pinMode(WATER_PUMP_PIN,OUTPUT);
  pinMode(HEATER_PIN,OUTPUT);
  pinMode(BUZZER_PIN,OUTPUT);
  pinMode(MOTOR_PIN,OUTPUT);
  
  initialiseSerial();
  checkENVShieldInitialisation();
  connectToWiFiNetwork();
  
  mqtt.loop();
  connectToMQTT();


  payloadHeater["heater_status"] = heater_status;
  serializeJson(jsonDocHeater, msg, JSON_SIZE);
  Serial.println(msg);
  if (!mqtt.publish(MQTT_TOPIC, msg)) {
   Serial.println("MQTT Publish failed");
  }
  
  payloadPump["pump_status"] = pump_status;
  serializeJson(jsonDocPump, msg, JSON_SIZE);
  Serial.println(msg);
  if (!mqtt.publish(MQTT_TOPIC, msg)) {
   Serial.println("MQTT Publish failed");
  }
}


void loop() {
  // Read from sensors
  float temp = ENV.readTemperature();
  float humi = ENV.readHumidity();
  float illu = ENV.readIlluminance();
  int sound = analogRead(MIC_PIN);
  float water = analogRead(WATER_LEVEL_PIN);

  // Check if any reads failed
  if (isnan(temp) || isnan(humi) || isnan(illu)) {
    Serial.println("Failed to read data!");
  } else {

    // Prepare data
    status["temp"] = temp;
    status["humi"] = humi;
    status["illu"] = illu;
    status["sound"] = sound;
    status["water"] = water;

    serializeJson(jsonDoc, msg, JSON_SIZE);
    Serial.println(msg);

    // Send data to Watson IoT Platform
    if (!mqtt.publish(MQTT_TOPIC, msg)) {
      Serial.println("MQTT Publish failed");
    }
  }

  // Pause - but keep polling MQTT for incoming messages
  for (int i = 0; i < 4; i++) {
    mqtt.loop();
    delay(1000);
  }

  mqtt.loop();
  connectToMQTT();
}



void connectToMQTT() {
  while (!mqtt.connected()) {
    if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
      Serial.println("MQTT Connected");
      mqtt.subscribe(MQTT_TOPIC_DISPLAY);
      mqtt.loop();
    } else {
      Serial.println("MQTT Failed to connect!");
      delay(5000);
    }
  }
}

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
