#include <WiFiNINA.h>
#include <Arduino_MKRENV.h>
#include "arduino_secrets.h"

#define PUMP_ON_TIME 5000

void printData();
void initialiseSerial();
void checkENVShieldInitialisation();
void connectToWiFiNetwork();
