#include <esp_sleep.h>
#include "wifi-connection.h"
#include "led-panel.h"
#include "dashboard.h"
#include "brightness.h"
#include "buzzer.h"
#include "pir-sensor.h"

LEDPanel ledPanel(8);
Dashboard dashboard(&ledPanel);
Brightness brightness(&ledPanel);

#define REFRESH_INTERVAL_MS  10000UL
#define SLEEP_TIMEOUT_MS     300000UL  // 5 minutes d'inactivité PIR

unsigned long lastRefresh = 0;
unsigned long lastActivity = 0;

void setup() {
  Serial.begin(115200);
  ledPanel.setup();
  dashboard.setup();
  dashboard.showLoading();
  WiFiConnection::setup();
  brightness.setup();
  Buzzer::setup();
  PIRSensor::setup();

  brightness.loop();
  dashboard.loop();
  lastRefresh = millis();
  lastActivity = millis();
}

void loop() {
  Buzzer::loop();

  unsigned long now = millis();

  if (PIRSensor::isTriggered()) {
    lastActivity = now;
  }

  if (now - lastActivity >= SLEEP_TIMEOUT_MS) {
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
