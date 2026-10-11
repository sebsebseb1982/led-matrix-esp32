#include <math.h>
#include <esp_attr.h>
#include "weather-service.h"
#include "home-assistant.h"
#include "buzzer.h"

// Prevision Meteo-France : plus locale que met.no (weather.forecast_maison),
// et 78 h de prevision horaire contre 48.
#define FORECAST_ENTITY "weather.talence"

// ponytail: une colonne represente ~23 min. Refetcher l'historique, la
// prevision, le croisement et le soleil toutes les 10 s etait ~30x plus souvent
// qu'ils ne peuvent bouger. Seule la ventilation, qui declenche le buzzer,
// reste relue a chaque cycle.
#define SLOW_REFRESH_MS (5UL * 60 * 1000)

// Nombre de points bruts conserves pour la regression du croisement.
static const int CROSSING_MAX_PTS = 60;
// Horizon de recherche du croisement.
static const long CROSSING_HORIZON_SECS = 12L * 3600L;
// Points horaires de prevision conserves (au-dela de l'horizon, inutiles).
static const int FORECAST_MAX_PTS = 24;

static long  forecastTs[FORECAST_MAX_PTS];
static float forecastTemps[FORECAST_MAX_PTS];
// Ecart capteur - prevision a l'instant present, ajoute a toute la prevision :
// le capteur exterieur a son propre biais (exposition, abri), et sans ce
// recalage la courbe ferait une marche a la colonne NOW_COL.
static float forecastBias = 0.0f;

// Modele thermique de l'etage, en degC/s, ajuste par moindres carres :
//   dTin/dt = a * (Text - Tin) + b * chauffe + c
// Sur 7 jours d'historique reel, ajuste sur 48 h il prevoit l'etage a +6 h a
// 0,23 degC pres (erreur max 0,59), contre 0,26 / 0,85 pour une tendance
// lineaire ; ajuste sur 18 h seulement, il fait moins bien qu'une valeur figee.
// Un terme ventilation ou ensoleillement degradait le resultat.
// Coefficients en RTC : un reveil PIR ne relance pas les 12 requetes du fit.
static const long MODEL_HOURS       = 48;
static const int  MODEL_COLS        = 126;  // ~23 min par case, comme l'affichage
static const long MODEL_CHUNK_HOURS = 12;   // 4 requetes par serie au lieu de 12
static const long MODEL_REFIT_SECS  = 3600;
RTC_DATA_ATTR static float modelA     = NAN;
RTC_DATA_ATTR static float modelB     = 0.0f;
RTC_DATA_ATTR static float modelC     = 0.0f;
RTC_DATA_ATTR static long  modelFitTs = 0;

// Trajectoire simulee de l'etage, de maintenant a l'horizon du croisement.
static const long INTERIOR_STEP_SECS = 5 * 60;
static const int  INTERIOR_STEPS     = CROSSING_HORIZON_SECS / INTERIOR_STEP_SECS;
static float interiorTraj[INTERIOR_STEPS + 1];
static long  interiorStartTs = 0;
static bool  interiorValid   = false;

float WeatherService::etageTemps[SCREEN_WIDTH];
float WeatherService::extTemps[SCREEN_WIDTH];
int   WeatherService::etageValid = 0;
int   WeatherService::extValid   = 0;
int   WeatherService::forecastCount = 0;
bool  WeatherService::ventIsOn   = false;
float WeatherService::lastEtage      = NAN;
float WeatherService::lastExt        = NAN;
float WeatherService::crossingMinutes = NAN;
int   WeatherService::sunriseX = -1;
int   WeatherService::sunsetX  = -1;
bool  WeatherService::heatingOn[SCREEN_WIDTH] = {};

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

// Prevision interpolee a l'instant ts, recalee sur le capteur. Avant le premier
// point (heure entamee), on prend le premier ; NAN apres le dernier.
float WeatherService::forecastAt(long ts) {
  int n = forecastCount;
  if (n == 0) return NAN;
  if (ts <= forecastTs[0]) return forecastTemps[0] + forecastBias;
  for (int i = 1; i < n; i++) {
    if (ts <= forecastTs[i]) {
      float t = (float)(ts - forecastTs[i - 1]) / (float)(forecastTs[i] - forecastTs[i - 1]);
      return forecastTemps[i - 1] + t * (forecastTemps[i] - forecastTemps[i - 1]) + forecastBias;
    }
  }
  return NAN;
}

// A appeler apres l'historique : le recalage a besoin de lastExt.
void WeatherService::fetchForecast() {
  forecastCount = HomeAssistant::getHourlyForecast(FORECAST_ENTITY, forecastTs, forecastTemps, FORECAST_MAX_PTS);

  long nowTs = (long)time(NULL);
  forecastBias = 0.0f;
  float rawNow = forecastAt(nowTs);
  if (!isnan(rawNow) && !isnan(lastExt)) forecastBias = lastExt - rawNow;
  Serial.printf("[weather] prevision: %d pts, brut maintenant=%.1f, biais=%+.1f\n",
                forecastCount, rawNow, forecastBias);

  for (int x = NOW_COL + 1; x < SCREEN_WIDTH; x++)
    extTemps[x] = forecastAt(nowTs + (long)((x - NOW_COL) * SECS_PER_COL));
}

// Resout le systeme 3x3 m * x = m[.][3] (pivot partiel). false si singulier.
static bool solve3(double m[3][4], double x[3]) {
  for (int col = 0; col < 3; col++) {
    int piv = col;
    for (int r = col + 1; r < 3; r++)
      if (fabs(m[r][col]) > fabs(m[piv][col])) piv = r;
    if (fabs(m[piv][col]) < 1e-12) return false;
    for (int c = 0; c < 4; c++) { double t = m[col][c]; m[col][c] = m[piv][c]; m[piv][c] = t; }
    for (int r = 0; r < 3; r++) {
      if (r == col) continue;
      double f = m[r][col] / m[col][col];
      for (int c = col; c < 4; c++) m[r][c] -= f * m[col][c];
    }
  }
  for (int i = 0; i < 3; i++) x[i] = m[i][3] / m[i][i];
  return true;
}

bool WeatherService::fitInteriorModel() {
  static float tin[MODEL_COLS], tex[MODEL_COLS];
  static bool  heat[MODEL_COLS];

  int nIn = HomeAssistant::getTimeSeries("sensor.domo_etage", MODEL_HOURS, tin, MODEL_COLS, MODEL_CHUNK_HOURS);
  int nEx = HomeAssistant::getTimeSeries("sensor.domo_ext_rieur", MODEL_HOURS, tex, MODEL_COLS, MODEL_CHUNK_HOURS);
  HomeAssistant::getHeatingColumns("climate.thermostat", MODEL_HOURS, heat, MODEL_COLS, MODEL_CHUNK_HOURS);
  if (nIn < 2 || nEx < 2) {
    Serial.println("[model] ABANDON: historique 48 h indisponible");
    return false;
  }
  interpolate(tin, MODEL_COLS);
  interpolate(tex, MODEL_COLS);

  // Equations normales de la regression (x = [Text - Tin, chauffe, 1],
  // y = pente de Tin), avec une legere regularisation : sans chauffe sur la
  // fenetre, la colonne chauffe est nulle et b doit rester a 0.
  const double dt = MODEL_HOURS * 3600.0 / (MODEL_COLS - 1);
  double m[3][4] = {};
  int rows = 0;
  for (int i = 0; i + 1 < MODEL_COLS; i++) {
    if (isnan(tin[i]) || isnan(tin[i + 1]) || isnan(tex[i])) continue;
    double x[3] = { tex[i] - tin[i], heat[i] ? 1.0 : 0.0, 1.0 };
    double y = (tin[i + 1] - tin[i]) / dt;
    for (int r = 0; r < 3; r++) {
      for (int c = 0; c < 3; c++) m[r][c] += x[r] * x[c];
      m[r][3] += x[r] * y;
    }
    rows++;
  }
  if (rows < 24) {
    Serial.printf("[model] ABANDON: %d lignes seulement\n", rows);
    return false;
  }
  for (int r = 0; r < 3; r++) m[r][r] += 1e-3;

  double w[3];
  if (!solve3(m, w)) {
    Serial.println("[model] ABANDON: systeme singulier");
    return false;
  }
  // a <= 0 voudrait dire que l'etage s'eloigne de l'exterieur : fit aberrant.
  if (!(w[0] > 0)) {
    Serial.printf("[model] ABANDON: a=%.2e non physique\n", w[0]);
    return false;
  }
  modelA = (float)w[0];
  modelB = (float)max(w[1], 0.0);
  modelC = (float)w[2];
  Serial.printf("[model] %d lignes: a=%.3f/h  b=%+.2f degC/h  c=%+.2f degC/h\n",
                rows, modelA * 3600.0f, modelB * 3600.0f, modelC * 3600.0f);
  return true;
}

// Simule l'etage depuis la derniere mesure, avec l'exterieur prevu.
void WeatherService::simulateInterior() {
  interiorValid = false;
  if (isnan(modelA) || forecastCount == 0 || isnan(lastEtage)) return;

  bool  heatMode = false;
  float setpoint = NAN;
  if (!HomeAssistant::getClimateState("climate.thermostat", heatMode, setpoint))
    heatMode = false;

  long  nowTs = (long)time(NULL);
  float t     = lastEtage;
  interiorTraj[0] = t;
  for (int k = 1; k <= INTERIOR_STEPS; k++) {
    float ext = forecastAt(nowTs + (long)(k - 1) * INTERIOR_STEP_SECS);
    if (isnan(ext)) return;
    float heat = (heatMode && t < setpoint) ? 1.0f : 0.0f;
    float next = t + (modelA * (ext - t) + modelB * heat + modelC) * INTERIOR_STEP_SECS;
    // Le thermostat tient la consigne. Sans chauffe dans la fenetre
    // d'apprentissage, b est nul et la prevision passerait dessous.
    if (heatMode && t >= setpoint && next < setpoint) next = setpoint;
    interiorTraj[k] = t = next;
  }
  interiorStartTs = nowTs;
  interiorValid   = true;
  Serial.printf("[model] etage prevu: %.1f -> %.1f (+6h) -> %.1f (+12h)\n",
                interiorTraj[0], interiorTraj[INTERIOR_STEPS / 2], interiorTraj[INTERIOR_STEPS]);
}

float WeatherService::interiorAt(long ts) {
  if (!interiorValid) return NAN;
  float f = (float)(ts - interiorStartTs) / INTERIOR_STEP_SECS;
  if (f < 0 || f > INTERIOR_STEPS) return NAN;
  int i = (int)f;
  if (i >= INTERIOR_STEPS) return interiorTraj[INTERIOR_STEPS];
  return interiorTraj[i] + (f - i) * (interiorTraj[i + 1] - interiorTraj[i]);
}

// Balayage par pas de 5 min jusqu'au changement de signe de diffAt(ts)
// (etage - exterieur), puis interpolation lineaire dans le pas.
template <typename F>
static float scanCrossing(F diffAt, long nowTs, const char* source) {
  const long STEP = 5 * 60;
  double prev = diffAt(nowTs);
  for (long dt = STEP; dt <= CROSSING_HORIZON_SECS; dt += STEP) {
    double d = diffAt(nowTs + dt);
    if (isnan(prev) || isnan(d)) {
      Serial.println("[cross] ABANDON: prevision trop courte");
      return NAN;
    }
    if ((prev > 0) != (d > 0)) {
      double t_cross = (dt - STEP) + STEP * prev / (prev - d);
      Serial.printf("[cross] OK (%s): croisement dans %.1fmin\n", source, t_cross / 60.0);
      return (float)(t_cross / 60.0);
    }
    prev = d;
  }
  Serial.println("[cross] ABANDON: pas de croisement prevu dans les 12h");
  return NAN;
}

float WeatherService::estimateCrossingMinutes() {
  // Etage simule contre exterieur prevu : aucune requete supplementaire.
  if (interiorValid)
    return scanCrossing([](long ts) -> double { return interiorAt(ts) - forecastAt(ts); },
                        (long)time(NULL), "modele");

  const long FETCH_HOURS   = 3;
  const long REGRESS_SECS  = 160 * 60;  // regression sur la derniere 2h40 (x2)
  const int  MAX_PTS       = CROSSING_MAX_PTS;

  static long  etageTs[MAX_PTS], extTs[MAX_PTS];
  static float etageVals[MAX_PTS], extVals[MAX_PTS];

  // Avec une prevision, l'exterieur n'a pas besoin d'etre extrapole : seule
  // la tendance de l'etage est estimee, d'ou une requete en moins.
  bool useForecast = forecastCount > 0;

  int ne = HomeAssistant::getRawSeries("sensor.domo_etage", FETCH_HOURS, etageTs, etageVals, MAX_PTS);
  int nx = useForecast ? 0
         : HomeAssistant::getRawSeries("sensor.domo_ext_rieur", FETCH_HOURS, extTs, extVals, MAX_PTS);

  Serial.printf("[cross] etage=%d pts, ext=%d pts (sur %dh), prevision=%s\n",
                ne, nx, (int)FETCH_HOURS, useForecast ? "oui" : "non");
  if (ne < 2 || (!useForecast && nx < 2)) {
    Serial.println("[cross] ABANDON: pas assez de points");
    return NAN;
  }

  // tRef = timestamp le plus recent des capteurs lus
  long tRef    = useForecast ? etageTs[ne - 1] : max(etageTs[ne - 1], extTs[nx - 1]);
  long tCutoff = tRef - REGRESS_SECS;
  long nowTs   = (long)time(NULL);
  Serial.printf("[cross] tRef=now-%lds  fenetre regression: [now-%lds .. now-%lds]\n",
                nowTs - tRef, nowTs - tCutoff, nowTs - tRef);

  // Trouver le premier point dans la fenetre de regression pour chaque capteur
  int e0 = 0; while (e0 < ne && etageTs[e0] < tCutoff) e0++;
  int x0 = 0; while (x0 < nx && extTs[x0]   < tCutoff) x0++;
  int ne_r = ne - e0, nx_r = nx - x0;

  // Capteur etage tres peu bavard: fallback sur toute la fenetre FETCH_HOURS si la fenetre courte est vide
  if (ne_r < 2) {
    Serial.printf("[cross] etage: fenetre vide, fallback sur %dh complet\n", (int)FETCH_HOURS);
    e0 = 0;
    ne_r = ne;
  }

  Serial.printf("[cross] pts dans fenetre: etage=%d, ext=%d\n", ne_r, nx_r);
  if (ne_r < 2 || (!useForecast && nx_r < 3)) {
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

  double ae, be;
  if (!linFit(etageTs + e0, etageVals + e0, ne_r, tRef, ae, be)) {
    Serial.println("[cross] ABANDON: linFit etage degenere");
    return NAN;
  }
  Serial.printf("[cross] regression: etage=%.2f + %.5f*t (%.3f°C/h)\n", be, ae, ae * 3600.0);

  if (useForecast) {
    // Modele indisponible : etage extrapole lineairement contre exterieur prevu.
    return scanCrossing([&](long ts) -> double {
      float ext = forecastAt(ts);
      return isnan(ext) ? NAN : be + ae * (double)(ts - tRef) - ext;
    }, nowTs, "tendance");
  }

  // Sans prevision : extrapolation lineaire des deux capteurs.
  double ax, bx;
  if (!linFit(extTs + x0, extVals + x0, nx_r, tRef, ax, bx)) {
    Serial.println("[cross] ABANDON: linFit ext degenere");
    return NAN;
  }
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
  static unsigned long lastSlowMs = 0;

  // Historique, previsions, croisement et positions solaires : tout ce qui ne
  // peut pas bouger en moins de quelques minutes.
  if (lastSlowMs == 0 || millis() - lastSlowMs >= SLOW_REFRESH_MS) {
    // L'historique n'occupe que les colonnes 0..NOW_COL, la suite est a la prevision.
    Serial.println("[weather] fetch temperature_etage...");
    etageValid = HomeAssistant::getTimeSeries("sensor.domo_etage", HISTORY_HOURS, etageTemps, NOW_COL + 1);
    Serial.printf("[weather] etage: %d points valides\n", etageValid);

    Serial.println("[weather] fetch domo_ext_rieur...");
    extValid = HomeAssistant::getTimeSeries("sensor.domo_ext_rieur", HISTORY_HOURS, extTemps, NOW_COL + 1);
    Serial.printf("[weather] ext: %d points valides\n", extValid);

    // Derniere valeur lue avant interpolation, tant que les NAN distinguent
    // encore une mesure reelle d'une case bouchee.
    lastEtage = NAN;
    lastExt   = NAN;
    for (int i = NOW_COL; i >= 0; i--) {
      if (isnan(lastEtage) && !isnan(etageTemps[i])) lastEtage = etageTemps[i];
      if (isnan(lastExt)   && !isnan(extTemps[i]))   lastExt   = extTemps[i];
      if (!isnan(lastEtage) && !isnan(lastExt)) break;
    }
    Serial.printf("[weather] derniere valeur: etage=%.1f ext=%.1f\n", lastEtage, lastExt);

    interpolate(etageTemps, NOW_COL + 1);
    interpolate(extTemps, NOW_COL + 1);

    Serial.println("[weather] fetch prevision...");
    fetchForecast();

    long nowTs = (long)time(NULL);
    if (isnan(modelA) || nowTs - modelFitTs >= MODEL_REFIT_SECS) {
      Serial.println("[weather] ajustement du modele etage (48 h)...");
      // En cas d'echec, les anciens coefficients restent ; nouvel essai au
      // prochain cycle lent.
      if (fitInteriorModel()) modelFitTs = nowTs;
    }
    simulateInterior();
    for (int x = NOW_COL + 1; x < SCREEN_WIDTH; x++)
      etageTemps[x] = interiorAt(nowTs + (long)((x - NOW_COL) * SECS_PER_COL));

    Serial.println("[weather] fetch climate.thermostat...");
    HomeAssistant::getHeatingColumns("climate.thermostat", HISTORY_HOURS, heatingOn, NOW_COL + 1);

    crossingMinutes = estimateCrossingMinutes();
    crossingUnixTs  = !isnan(crossingMinutes)
                      ? (long)time(NULL) + (long)(crossingMinutes * 60.0f)
                      : 0L;
    if (crossingUnixTs)
      Serial.printf("[weather] wakeup timer prevu a unix=%ld (dans %.1fmin)\n", crossingUnixTs, crossingMinutes);

    Serial.println("[weather] fetch sun times...");
    time_t nextRising = 0, nextSetting = 0;
    if (HomeAssistant::getSunTimes(nextRising, nextSetting)) {
      long windowStart = nowTs - (long)(NOW_COL * SECS_PER_COL);
      long windowEnd   = nowTs + (long)((SCREEN_WIDTH - 1 - NOW_COL) * SECS_PER_COL);

      // next_rising/next_setting tombent dans les 24 h a venir, et la fenetre
      // affichee couvre ~24 h : l'evenement y figure soit tel quel (dans la
      // partie prevision), soit tel qu'il s'est produit la veille.
      auto toX = [&](time_t t) -> int {
        long ts = (long)t;
        if (ts > windowEnd) ts -= 24L * 3600L;
        if (ts < windowStart || ts > windowEnd) return -1;
        return (int)lroundf((ts - windowStart) / SECS_PER_COL);
      };

      sunriseX = toX(nextRising);
      sunsetX  = toX(nextSetting);
      Serial.printf("[weather] sunriseX=%d sunsetX=%d\n", sunriseX, sunsetX);
    } else {
      sunriseX = -1;
      sunsetX  = -1;
    }

    lastSlowMs = millis();
  }

  // Ventilation : relue a chaque cycle, c'est le seul etat qui doit etre vu vite
  // (il declenche le buzzer).
  Serial.println("[weather] fetch etat_ventilation...");
  String ventStr = HomeAssistant::getEntityState("input_boolean.etat_ventilation");
  Serial.printf("[weather] ventilation: %s\n", ventStr.c_str());

  // "?" = appel echoue. Le lire comme "off" ferait biper le buzzer a chaque
  // coupure reseau : on garde l'etat precedent.
  if (ventStr != "?") {
    bool newVentIsOn = (ventStr == "on");
    if (lastVentilationState != -1 && (bool)lastVentilationState != newVentIsOn)
      Buzzer::beepbeepbeep(newVentIsOn ? 50 : 200);
    lastVentilationState = newVentIsOn ? 1 : 0;
    ventIsOn = newVentIsOn;
  }
}
