#pragma once
#include "globals.h"
#include "arduino_secrets.h"
#include "setup.h"
#include <string.h>
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Arduino_MKRENV.h>


bool isWaterContainerFull()
{
    int water_level = getWaterLevelReading();
    // validate the reading:
    if (water_level < 0 || water_level > 1023)
        return true; // cannot pour as sensor malfunctioned!
    if (water_level < WATER_MAX_LEVEL)
    {
#if (DEBUG > 1)
        Serial.println("Water level below MAX, can pour!");
#endif
        return false;
    }
#if (DEBUG > 1)
        Serial.println("Water level above or equal MAX, cannot pour!");
#endif
    return true;
}

bool isWaterContainerEmpty()
{
    int water_level = getWaterLevelReading();
    // validate the reading:
    if (water_level < 0 || water_level > 1023)
        return false; // cannot pour as sensor malfunctioned!
    if (getWaterLevelReading() < WATER_MIN_LEVEL)
    {
#if (DEBUG > 1)
        Serial.println("Water level below MIN, need to pour!");
#endif
        return true;
    }
#if (DEBUG > 1)
        Serial.println("Water level above or equal MIN, no need to pour!");
#endif
    return false;
}

int getWaterLevelReading()
{
    float avg = 0;
    for (int i = 0; i < WATER_LEVEL_SENSOR_READINGS; ++i)
    {
        avg += analogRead(WATER_LEVEL_PIN);
        delay(READING_INTERVAL);
    }
    return avg/WATER_LEVEL_SENSOR_READINGS;
}


void TimerHandler()
{
    ISR_Timer.run();
}

void pumpTimerHandler()
{
#if (DEBUG > 1)
    Serial.println(millis()-preMillisTimer);
    preMillisTimer = millis();
    Serial.println("Disabling pump!");
#endif
    pump_status = false;
    digitalWrite(WATER_PUMP_PIN, LOW);
    sendPumpStatus();
}

void heaterTimerHandler()
{
#if (DEBUG > 1)
    Serial.println("Disabling heater! too long on!");
#endif
    heaterTimerId = -1; // Reset the timerId to -1, so other timers dont get accidentially removed
    heater_status = false;
    digitalWrite(HEATER_PIN, LOW);
    sendHeaterStatus();
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
      // If heater msg is Off, that means it is currently off and need to be toggled on
    if (strcmp((const char*) callbackDoc["heater"], "Off") == 0)
    {
        heater_status = true;
        // In case the Arduino somehow receives two 'Off' messages (e.g. bug on website),
        // Handle it gracefully, and remove previous timer
        if (heaterTimerId != -1)
        {
#if (DEBUG > 1)
            Serial.println("Removing heaterTimer interupt!");
#endif
            ISR_Timer.deleteTimer(heaterTimerId);
            heaterTimerId = -1;
        }
        // set a safety timeout, triggering after HEATER_TIMEOUT MS
        heaterTimerId = ISR_Timer.setTimeout(HEATER_TIMEOUT, heaterTimerHandler);
        digitalWrite(HEATER_PIN,HIGH);
    }
    else
    {
        heater_status = false;
        if (heaterTimerId != -1)
        {
#if (DEBUG > 1)
            Serial.println("Removing heaterTimer interupt!");
#endif
            ISR_Timer.deleteTimer(heaterTimerId);
            heaterTimerId = -1;
        }
        digitalWrite(HEATER_PIN, LOW);
    }
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
        if (isWaterContainerFull())
        {
            pump_status = false;
            sendPumpStatus();
            return;
        }
        pump_status = true;
#if (DEBUG > 1)
        preMillisTimer = millis();
#endif
        digitalWrite(WATER_PUMP_PIN,HIGH);
        ISR_Timer.setTimeout(PUMP_ON_TIME, pumpTimerHandler);
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
  ITimer0.attachInterruptInterval(HW_TIMER_INTERVAL_MS * 1000, TimerHandler);
}


void loop() {
  // Read from sensors
  float temp = ENV.readTemperature();
  float humi = ENV.readHumidity();
  float illu = ENV.readIlluminance();
  int sound = analogRead(MIC_PIN);
  float water = getWaterLevelReading();

  // Check if any reads failed
  if (isnan(temp) || isnan(humi) || isnan(illu)) {
#if (DEBUG > 0)
    Serial.println("Failed to read data!");
#endif
  } else {
    // check if water container empty
    if (isWaterContainerEmpty())
    {
    }

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
