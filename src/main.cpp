#include <esp_sleep.h>
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

#define REFRESH_INTERVAL_MS  10000UL
#define DISPLAY_DURATION_MS  60000UL

unsigned long lastRefresh = 0;

void setup() {
  Serial.begin(115200);
  WiFiConnection::setup();
  OTA::setup();
  ledPanel.setup();
  dashboard.setup();
  brightness.setup();
  Buzzer::setup();
  PIRSensor::setup();

  // Premier affichage immédiat au démarrage
  brightness.loop();
  dashboard.loop();
  lastRefresh = millis();

  // Deep sleep désactivé - rafraîchissement toutes les 10s dans loop()
  // Serial.println("Rentre en mode Deep Sleep");
  // esp_deep_sleep_start();
}

void loop() {
  OTA::loop();
  Buzzer::loop();

  unsigned long now = millis();

  if (now >= DISPLAY_DURATION_MS) {
    Serial.println("[main] Deep sleep...");
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, HIGH);
    esp_deep_sleep_start();
  }

  if (now - lastRefresh >= REFRESH_INTERVAL_MS) {
    lastRefresh = now;
    brightness.loop();
    dashboard.loop();
  }
}
