#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include "led-panel.h"

// Axe X : colonnes 0..NOW_COL = historique des HISTORY_HOURS dernieres heures,
// NOW_COL+1..SCREEN_WIDTH-1 = prevision exterieure, a la meme echelle
// (~23 min par colonne, soit ~6 h de prevision).
#define HISTORY_HOURS 18
#define NOW_COL       47
#define SECS_PER_COL  (HISTORY_HOURS * 3600.0f / NOW_COL)

class WeatherService {
public:
  static float etageTemps[SCREEN_WIDTH];  // prevision (modele) au-dela de NOW_COL (NAN si indisponible)
  static float extTemps[SCREEN_WIDTH];    // prevision au-dela de NOW_COL (NAN si indisponible)
  static int   etageValid;
  static int   extValid;
  static int   forecastCount;             // points de prevision recus, 0 si echec
  static bool  ventIsOn;
  static float lastEtage;
  static float lastExt;
  static float crossingMinutes;  // NAN si pas de croisement prevu dans les 12h
  static int   sunriseX;         // pixel X du lever du soleil, -1 si hors fenetre
  static int   sunsetX;          // pixel X du coucher du soleil, -1 si hors fenetre
  static bool  heatingOn[SCREEN_WIDTH];  // chauffage actif sur la colonne X

  static void refresh();
  static float estimateCrossingMinutes();
  static long getCrossingUnixTs();

private:
  static void interpolate(float* series, int size);
  static void fetchForecast();
  static float forecastAt(long ts);
  static bool  fitInteriorModel();
  static void  simulateInterior();
  static float interiorAt(long ts);
};

#endif
