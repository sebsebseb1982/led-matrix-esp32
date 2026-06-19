#include "pir-sensor.h"
#include <Arduino.h>

#define PIR_SENSOR_PIN 33

void PIRSensor::setup() {
  pinMode(PIR_SENSOR_PIN, INPUT); 
}

bool PIRSensor::isTriggered() {
  return digitalRead(PIR_SENSOR_PIN) == HIGH;
}
