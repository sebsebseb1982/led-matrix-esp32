#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include "led-panel.h"

class WeatherService {
public:
  static float etageTemps[SCREEN_WIDTH];
  static float extTemps[SCREEN_WIDTH];
  static int   etageValid;
  static int   extValid;
  static bool  ventIsOn;
  static float lastEtage;
  static float lastExt;
  static float crossingMinutes;  // NAN si pas de croisement prevu dans les 12h
  static int   sunriseX;         // pixel X du lever du soleil d'hier, -1 si inconnu
  static int   sunsetX;          // pixel X du coucher du soleil d'hier, -1 si inconnu

  static void refresh();
  static float estimateCrossingMinutes();
  static long getCrossingUnixTs();

private:
  static void interpolate(float* series, int size);
};

#endif
