#include "arduino_secrets.h"
// Timer lib
#include <SAMDTimerInterrupt.h>
#include <SAMD_ISR_Timer.h>

// Wifi and MQTT libs
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// sensors and actuators
#include <Arduino_MKRENV.h>
#include <Servo.h>


// 0: no log messages, 1: safe log messages, 2: halting log messages
#define DEBUG                         2
// Time in MS the pump is on after receiving MQTT msg
#define PUMP_ON_TIME                  5000L
// MAX Time in MS the heater is on after receiving MQTT msg (if not turned off by user before)
#define HEATER_TIMEOUT                30*1000L
// Time for food dispensing in MS
#define FOOD_DISPENSING_TIMEOUT       5000L
// Interval of the main HW Clock in MS
#define HW_TIMER_INTERVAL_MS          50L
#define WATER_LEVEL_SENSOR_READINGS   10
#define READING_INTERVAL              2

#define WATER_MAX_LEVEL               680
#define WATER_MIN_LEVEL               400
#define WATER_SENSOR_MAX_VAL          800
// Servo positions
#define SERVO_CLOSED_POS              0
#define SERVO_OPEN_POS                180

const int JSON_SIZE = 1000;

const int HEATER_PIN = 0;
const int WATER_PUMP_PIN = 1;
const int MIC_PIN = A0;
const int BUZZER_PIN = A1;
const int WATER_LEVEL_PIN = A4;
const int MOTOR_PIN = A3;

int getWaterLevelReading();
bool isWaterContainerFull();
bool isWaterContainerEmpty();

Servo food_dispenser_servo;

// Values modifed by Interrupts
volatile int heaterTimerId = -1; // Stores the id of the current heaterTimer interrupt
volatile bool heater_status = false;
volatile bool pump_status = false;
volatile bool food_dispenser_status = false;
volatile uint32_t preMillisTimer = 0;

// ------------ MQTT related vars ------------------
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
// Main timer
SAMDTimerInterrupt ITimer0(TIMER_TCC);
// SAMD ISR timers, use main timer for up to 16 timers
SAMD_ISR_Timer ISR_Timer;
void TimerHandler();
void pumpTimerHandler();
void foodDispenserTimerHandler();
// ---------------------------------------------
