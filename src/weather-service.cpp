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
float WeatherService::lastEtage      = NAN;
float WeatherService::lastExt        = NAN;
float WeatherService::crossingMinutes = NAN;
int   WeatherService::sunriseX = -1;
int   WeatherService::sunsetX  = -1;

RTC_DATA_ATTR static int  lastVentilationState = -1;
RTC_DATA_ATTR static long crossingUnixTs       = 0;  // timestamp absolu du prochain croisement prevu

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

float WeatherService::estimateCrossingMinutes() {
  const long FETCH_HOURS   = 2;
  const long REGRESS_SECS  = 80 * 60;  // regression sur la derniere 1h20
  const int  MAX_PTS       = 60;

  static long  etageTs[MAX_PTS], extTs[MAX_PTS];
  static float etageVals[MAX_PTS], extVals[MAX_PTS];

  int ne = HomeAssistant::getRawSeries("sensor.domo_etage", FETCH_HOURS, etageTs, etageVals, MAX_PTS);
  int nx = HomeAssistant::getRawSeries("sensor.domo_ext_rieur",    FETCH_HOURS, extTs,   extVals,   MAX_PTS);

  Serial.printf("[cross] etage=%d pts, ext=%d pts (sur %dh)\n", ne, nx, (int)FETCH_HOURS);
  if (ne < 2 || nx < 2) {
    Serial.println("[cross] ABANDON: pas assez de points");
    return NAN;
  }

  // tRef = timestamp le plus recent des deux capteurs
  long tRef    = max(etageTs[ne - 1], extTs[nx - 1]);
  long tCutoff = tRef - REGRESS_SECS;
  long nowTs   = (long)time(NULL);
  Serial.printf("[cross] tRef=now-%lds  fenetre regression: [now-%lds .. now-%lds]\n",
                nowTs - tRef, nowTs - tCutoff, nowTs - tRef);

  // Trouver le premier point dans la fenetre de regression pour chaque capteur
  int e0 = 0; while (e0 < ne && etageTs[e0] < tCutoff) e0++;
  int x0 = 0; while (x0 < nx && extTs[x0]   < tCutoff) x0++;
  int ne_r = ne - e0, nx_r = nx - x0;

  // Capteur etage tres peu bavard: fallback sur toute la fenetre 2h si la fenetre courte est vide
  if (ne_r < 2) {
    Serial.println("[cross] etage: fenetre vide, fallback sur 2h complet");
    e0 = 0;
    ne_r = ne;
  }

  Serial.printf("[cross] pts dans fenetre: etage=%d, ext=%d\n", ne_r, nx_r);
  if (ne_r > 0)
    Serial.printf("[cross] etage: premier=%.2f(@now-%lds) dernier=%.2f(@now-%lds)\n",
                  etageVals[e0], nowTs - etageTs[e0],
                  etageVals[ne-1], nowTs - etageTs[ne-1]);
  if (nx_r > 0)
    Serial.printf("[cross] ext:   premier=%.2f(@now-%lds) dernier=%.2f(@now-%lds)\n",
                  extVals[x0], nowTs - extTs[x0],
                  extVals[nx-1], nowTs - extTs[nx-1]);

  if (ne_r < 2 || nx_r < 3) {
    Serial.println("[cross] ABANDON: pas assez de pts dans la fenetre");
    return NAN;
  }

  auto linFit = [](const long* ts, const float* vals, int n, long ref,
                   double& slope, double& intercept) -> bool {
    double sx = 0, sy = 0, sx2 = 0, sxy = 0;
    for (int i = 0; i < n; i++) {
      double t = (double)(ts[i] - ref);
      double y = (double)vals[i];
      sx += t; sy += y; sx2 += t * t; sxy += t * y;
    }
    double d = (double)n * sx2 - sx * sx;
    if (fabs(d) < 1e-6) return false;
    slope     = ((double)n * sxy - sx * sy) / d;
    intercept = (sy - slope * sx) / (double)n;
    return true;
  };

  double ae, be, ax, bx;
  if (!linFit(etageTs + e0, etageVals + e0, ne_r, tRef, ae, be)) {
    Serial.println("[cross] ABANDON: linFit etage degenere");
    return NAN;
  }
  if (!linFit(extTs + x0, extVals + x0, nx_r, tRef, ax, bx)) {
    Serial.println("[cross] ABANDON: linFit ext degenere");
    return NAN;
  }

  Serial.printf("[cross] regression: etage=%.2f + %.5f*t (%.3f°C/h)\n", be, ae, ae * 3600.0);
  Serial.printf("[cross] regression: ext  =%.2f + %.5f*t (%.3f°C/h)\n", bx, ax, ax * 3600.0);

  // Croisement : ae*t + be = ax*t + bx  =>  t = (bx - be) / (ae - ax)  (secondes depuis tRef)
  double da = ae - ax;
  Serial.printf("[cross] da=%.6f (etage-ext pente diff: %.4f°C/h)\n", da, da * 3600.0);
  if (fabs(da) < 1e-9) {
    Serial.println("[cross] ABANDON: pentes paralleles");
    return NAN;
  }
  double t_cross = (bx - be) / da;

  Serial.printf("[cross] t_cross=%.0fs (%.1fmin / %.2fh)\n", t_cross, t_cross / 60.0, t_cross / 3600.0);
  if (t_cross <= 0.0) {
    Serial.println("[cross] ABANDON: croisement dans le passe");
    return NAN;
  }
  if (t_cross > 12.0 * 3600.0) {
    Serial.printf("[cross] ABANDON: croisement trop loin (> 12h)\n");
    return NAN;
  }
  Serial.printf("[cross] OK: croisement dans %.1fmin\n", t_cross / 60.0);
  return (float)(t_cross / 60.0);
}

long WeatherService::getCrossingUnixTs() { return crossingUnixTs; }

void WeatherService::refresh() {
  Serial.println("[weather] fetch temperature_etage...");
  etageValid = HomeAssistant::getTimeSeries("sensor.domo_etage", HISTORY_HOURS, etageTemps, SCREEN_WIDTH);
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

  crossingMinutes = estimateCrossingMinutes();
  crossingUnixTs  = !isnan(crossingMinutes)
                    ? (long)time(NULL) + (long)(crossingMinutes * 60.0f)
                    : 0L;
  if (crossingUnixTs)
    Serial.printf("[weather] wakeup timer prevu a unix=%ld (dans %.1fmin)\n", crossingUnixTs, crossingMinutes);

  Serial.println("[weather] fetch sun times...");
  time_t nextRising = 0, nextSetting = 0;
  if (HomeAssistant::getSunTimes(nextRising, nextSetting)) {
    long nowTs       = (long)time(NULL);
    long windowStart = nowTs - 24L * 3600L;
    long windowLen   = 24L * 3600L;

    auto toX = [&](time_t t) -> int {
      long offset = (long)t - 24L * 3600L - windowStart;
      if (offset < 0 || offset > windowLen) return -1;
      return (int)(offset * (SCREEN_WIDTH - 1) / windowLen);
    };

    sunriseX = toX(nextRising);
    sunsetX  = toX(nextSetting);
    Serial.printf("[weather] sunriseX=%d sunsetX=%d\n", sunriseX, sunsetX);
  } else {
    sunriseX = -1;
    sunsetX  = -1;
  }
}
