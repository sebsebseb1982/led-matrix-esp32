#include <esp_sleep.h>
#include <time.h>
#include "wifi-connection.h"
#include "led-panel.h"
#include "dashboard.h"
#include "brightness.h"
#include "buzzer.h"
#include "pir-sensor.h"
#include "weather-service.h"

LEDPanel ledPanel;
Dashboard dashboard(&ledPanel);
Brightness brightness(&ledPanel);

#define REFRESH_INTERVAL_MS  10000UL
#define SLEEP_TIMEOUT_MS     180000UL  // 3 minutes d'inactivité PIR

unsigned long lastRefresh = 0;
unsigned long lastActivity = 0;

void setup() {
  Serial.begin(115200);
  ledPanel.setup();
  dashboard.showLoading();
  WiFiConnection::setup();
  Buzzer::setup();
  PIRSensor::setup();

  brightness.loop();
  dashboard.loop();
  lastRefresh = millis();
  lastActivity = millis();
}

void loop() {
  unsigned long now = millis();

  if (PIRSensor::isTriggered()) {
    lastActivity = now;
  }

  if (now - lastActivity >= SLEEP_TIMEOUT_MS) {
    Serial.println("[main] Deep sleep...");

    long nowUnix   = (long)time(NULL);
    long wakeUnix  = WeatherService::getCrossingUnixTs();
    long sleepSecs = (wakeUnix > nowUnix) ? (wakeUnix - nowUnix) : 0L;
    // > 0 et non > 60 : un croisement imminent est justement celui qu'il ne
    // faut pas rater, or l'ancien seuil le laissait dormir jusqu'au prochain PIR.
    if (sleepSecs > 0) {
      Serial.printf("[main] Timer wakeup dans %ldmin pour croisement courbes\n", sleepSecs / 60);
      esp_sleep_enable_timer_wakeup((uint64_t)sleepSecs * 1000000ULL);
    }

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, HIGH);
    esp_deep_sleep_start();
  }

  if (now - lastRefresh >= REFRESH_INTERVAL_MS) {
    WiFiConnection::loop();
    brightness.loop();
    dashboard.loop();
    // Apres le refresh : il bloque plusieurs secondes, partir du "now" d'avant
    // ferait repartir un cycle immediatement des qu'il depasse l'intervalle.
    lastRefresh = millis();
  }
}
