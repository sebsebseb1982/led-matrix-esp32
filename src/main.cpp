#include <esp_sleep.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
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
#define BRIGHTNESS_INTERVAL_MS 1000UL
#define SLEEP_TIMEOUT_MS     180000UL  // 3 minutes d'inactivité PIR
// Le refresh tournait jusqu'ici dans loopTask (8 Ko de pile) : meme marge, plus un peu.
#define REFRESH_TASK_STACK   12288

unsigned long lastBrightness = 0;
unsigned long lastActivity = 0;

// Les requetes HA bloquent plusieurs secondes (jusqu'a ~30 s avec le fit du
// modele) : elles tournent dans une tache sur le core 0, a cote de la pile
// WiFi, pour que loop() anime le balayage sans interruption sur le core 1.
// Le verrou protege les donnees de WeatherService, ecrites au fil des requetes
// par refresh() et lues par le rendu.
static SemaphoreHandle_t dataMutex;
static volatile bool dataReady = false;  // un refresh s'est termine depuis le dernier rendu

static void refreshTask(void*) {
  for (;;) {
    WiFiConnection::loop();
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    WeatherService::refresh();
    dataReady = true;
    xSemaphoreGive(dataMutex);
    // Delai compte apres le refresh, qui peut depasser l'intervalle.
    vTaskDelay(pdMS_TO_TICKS(REFRESH_INTERVAL_MS));
  }
}

void setup() {
  Serial.begin(115200);
  ledPanel.setup();
  dashboard.showLoading();
  WiFiConnection::setup();
  Buzzer::setup();
  PIRSensor::setup();

  brightness.loop();
  lastBrightness = millis();
  lastActivity = millis();

  // L'icone de chargement reste affichee jusqu'au premier refresh.
  dataMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(refreshTask, "ha-refresh", REFRESH_TASK_STACK, nullptr, 1, nullptr, 0);
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

  // Sans attente : si le verrou est pris, la tache a deja entame le refresh
  // suivant et on redessinera a la fin de celui-ci.
  if (dataReady && xSemaphoreTake(dataMutex, 0) == pdTRUE) {
    dataReady = false;
    dashboard.update();
    xSemaphoreGive(dataMutex);
  }

  if (now - lastBrightness >= BRIGHTNESS_INTERVAL_MS) {
    brightness.loop();
    lastBrightness = now;
  }

  dashboard.animate();
}
