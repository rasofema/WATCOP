#pragma once
#include "arduino_secrets.h"
#include "setup.h"
#include <string.h>
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Arduino_MKRENV.h>

// Timer lib
#include <SAMDTimerInterrupt.h>
//#include <SAMDTimerInterrupt.hpp>
//#include <SAMD_ISR_Timer.h>
//#include <SAMD_ISR_Timer.hpp>
#define DEBUG 2

const int JSON_SIZE = 1000;

const int HEATER_PIN = 0;
const int WATER_PUMP_PIN = 1;
const int MIC_PIN = A0;
const int BUZZER_PIN = A1;
const int WATER_LEVEL_PIN = A2;
const int MOTOR_PIN = A3;

bool heater_status = false;
volatile bool pump_status = false;
volatile uint32_t preMillisTimer = 0;

// ------------ MQTT objects ------------------
void connectToMQTT();
void callback(char* topic, byte* payload, unsigned int length);
void sendHeaterStatus();
void sendPumpStatus();
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, callback, wifiClient);
// ---------------------------------------------

// ----------------- JSON variables ------------
StaticJsonDocument<JSON_SIZE> jsonDoc;
JsonObject payload = jsonDoc.to<JsonObject>();
JsonObject status = payload.createNestedObject("sensor_data");

StaticJsonDocument<JSON_SIZE> jsonDocHeater;
JsonObject payloadHeater = jsonDocHeater.to<JsonObject>();

StaticJsonDocument<JSON_SIZE> jsonDocPump;
JsonObject payloadPump = jsonDocPump.to<JsonObject>();

static char msg[JSON_SIZE];
StaticJsonDocument<128> callbackDoc;
DeserializationError error;
// ---------------------------------------------

// ------------------ Timer Objects ------------
SAMDTimerInterrupt pump_timer(TIMER_TCC);
void pumpTimerHandler();
// ---------------------------------------------

void pumpTimerHandler()
{
#if (DEBUG > 1)
    Serial.println(millis()-preMillisTimer);
    preMillisTimer = millis();
    Serial.println("Disabling pump!");
#endif
    pump_status = false;
    digitalWrite(WATER_PUMP_PIN, LOW);
    pump_timer.detachInterrupt();
    sendPumpStatus();
}

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
#if (DEBUG > 0)
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");
#endif

  payload[length] = 0; // ensure valid content is zero terminated so can treat as c-string
#if (DEBUG > 0)
  Serial.println((char *)payload);
#endif
  error = deserializeJson(callbackDoc, (char *) payload);
  if (error) {
#if (DEBUG > 0)
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
#endif
    return;
  }
#if (DEBUG > 0)
  Serial.println((const char*)callbackDoc["heater"]);
#endif
  // Check whether the sent message is heater
  if (callbackDoc.containsKey("heater"))
  {
      // If heater status is On, that means it is currently on and need to be toggled off
    if (strcmp((const char*) callbackDoc["heater"], "On") == 0)
        heater_status = false;
    else
        heater_status = true;
#if (DEBUG > 0)
    Serial.println(heater_status);
#endif
    sendHeaterStatus();
    return;
  }
  // Check whether the sent message is pump 
  if (callbackDoc.containsKey("water_pump"))
  {
    if (strcmp((const char*) callbackDoc["water_pump"], "Pour") == 0)
    {
        pump_status = true;
        preMillisTimer = millis();
        digitalWrite(WATER_PUMP_PIN,HIGH);
        pump_timer.attachInterruptInterval(1000 * PUMP_ON_TIME, pumpTimerHandler);
    }
    else
        pump_status = false;
#if (DEBUG > 0)
    Serial.println(pump_status);
#endif
    sendPumpStatus();
    return;
  }
}

void sendHeaterStatus() {
  payloadHeater["heater_status"] = heater_status;
  serializeJson(jsonDocHeater, msg, JSON_SIZE);
#if (DEBUG > 0)
  Serial.println(msg);
  if (!mqtt.publish(MQTT_TOPIC, msg)) {
   Serial.println("MQTT Publish failed");
  }
#endif
}

void sendPumpStatus() {
  payloadPump["pump_status"] = pump_status;
  serializeJson(jsonDocPump, msg, JSON_SIZE);
#if (DEBUG > 0)
  Serial.println(msg);
  if (!mqtt.publish(MQTT_TOPIC, msg)) {
   Serial.println("MQTT Publish failed");
  }
#endif
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


  sendHeaterStatus();
  
  sendPumpStatus();
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
#if (DEBUG > 0)
    Serial.println("Failed to read data!");
#endif
  } else {

    // Prepare data
    status["temp"] = temp;
    status["humi"] = humi;
    status["illu"] = illu;
    status["sound"] = sound;
    status["water"] = water;

    serializeJson(jsonDoc, msg, JSON_SIZE);
#if (DEBUG > 0)
    Serial.println(msg);
    // Send data to Watson IoT Platform
    if (!mqtt.publish(MQTT_TOPIC, msg)) {
      Serial.println("MQTT Publish failed");
    }
#endif
  }

  // Pause - but keep polling MQTT for incoming messages
  // TODO may want to move this to a different thread/decrease the delay
  for (int i = 0; i < 4; i++) {
    mqtt.loop();
    delay(1000);
  }

  // Check if heater and pump should be on(values could have been updated by MQTT messages)
  if (heater_status)
    digitalWrite(HEATER_PIN,HIGH);
  else
    digitalWrite(HEATER_PIN,LOW);
    
  if (pump_status)
    digitalWrite(WATER_PUMP_PIN,HIGH);
  else
    digitalWrite(WATER_PUMP_PIN,LOW);
    
  mqtt.loop();
  connectToMQTT();
}

void connectToMQTT() {
  while (!mqtt.connected()) {
    if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
#if (DEBUG > 0)
      Serial.println("MQTT Connected");
#endif
      mqtt.subscribe(MQTT_TOPIC_DISPLAY);
      mqtt.loop();
    } else {
#if (DEBUG > 0)
      Serial.println("MQTT Failed to connect!");
#endif
      delay(5000);
    }
  }
}
