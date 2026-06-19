#include <math.h>
#include <esp_attr.h>
#include "weather-service.h"
#include "home-assistant.h"
#include "buzzer.h"

#define HISTORY_HOURS 24

float WeatherService::etageTemps[SCREEN_WIDTH];
float WeatherService::extTemps[SCREEN_WIDTH];
int   WeatherService::etageValid = 0;
int   WeatherService::extValid   = 0;
bool  WeatherService::ventIsOn   = false;
float WeatherService::lastEtage  = NAN;
float WeatherService::lastExt    = NAN;

RTC_DATA_ATTR static int lastVentilationState = -1;

void WeatherService::interpolate(float* series, int size) {
  int lastValid = -1;
  for (int i = 0; i < size; i++) {
    if (!isnan(series[i])) {
      if (lastValid >= 0 && i - lastValid > 1) {
        float v0 = series[lastValid], v1 = series[i];
        for (int j = lastValid + 1; j < i; j++) {
          float t = (float)(j - lastValid) / (i - lastValid);
          series[j] = v0 + t * (v1 - v0);
        }
      }
      lastValid = i;
    }
  }
  if (lastValid >= 0)
    for (int i = lastValid + 1; i < size; i++) series[i] = series[lastValid];
}

void WeatherService::refresh() {
  Serial.println("[weather] fetch temperature_etage...");
  etageValid = HomeAssistant::getTimeSeries("sensor.temperature_etage", HISTORY_HOURS, etageTemps, SCREEN_WIDTH);
  interpolate(etageTemps, SCREEN_WIDTH);
  Serial.printf("[weather] etage: %d points valides\n", etageValid);

  Serial.println("[weather] fetch domo_ext_rieur...");
  extValid = HomeAssistant::getTimeSeries("sensor.domo_ext_rieur", HISTORY_HOURS, extTemps, SCREEN_WIDTH);
  interpolate(extTemps, SCREEN_WIDTH);
  Serial.printf("[weather] ext: %d points valides\n", extValid);

  Serial.println("[weather] fetch etat_ventilation...");
  String ventStr = HomeAssistant::getEntityState("input_boolean.etat_ventilation");
  Serial.printf("[weather] ventilation: %s\n", ventStr.c_str());
  bool newVentIsOn = (ventStr == "on");
  if (lastVentilationState != -1 && (bool)lastVentilationState != newVentIsOn)
    Buzzer::beepbeepbeep(newVentIsOn ? 50 : 200);
  lastVentilationState = newVentIsOn ? 1 : 0;
  ventIsOn = newVentIsOn;

  lastEtage = NAN;
  lastExt   = NAN;
  for (int i = SCREEN_WIDTH - 1; i >= 0; i--) {
    if (isnan(lastEtage) && !isnan(etageTemps[i])) lastEtage = etageTemps[i];
    if (isnan(lastExt)   && !isnan(extTemps[i]))   lastExt   = extTemps[i];
    if (!isnan(lastEtage) && !isnan(lastExt)) break;
  }
  if (!isnan(lastEtage) && !isnan(lastExt))
    Serial.printf("[weather] derniere valeur: etage=%.1f ext=%.1f\n", lastEtage, lastExt);
}
