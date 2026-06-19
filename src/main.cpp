#include "wifi-connection.h"
#include "ota.h"
#include "led-panel.h"
#include "dashboard.h"
#include "brightness.h"
#include "buzzer.h"
#include "pir-sensor.h"

LEDPanel ledPanel(8);
Dashboard dashboard(&ledPanel);
Brightness brightness(&ledPanel);

void setup() {
  Serial.begin(115200);
  Serial.println("==============================");
  Serial.println("REBOOT apres deep sleep");
  Serial.println("==============================");
  WiFiConnection::setup();
  OTA::setup();
  ledPanel.setup();
  dashboard.setup();
  brightness.setup();
  Buzzer::setup();
  PIRSensor::setup();

  brightness.loop();
  dashboard.loop();

  Serial.println("");
  Serial.println("Rentre en mode Deep Sleep");
  Serial.println("----------------------");
  esp_deep_sleep_start();
}

void loop() {
}
