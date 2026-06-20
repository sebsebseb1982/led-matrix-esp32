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

  static void refresh();
  static float estimateCrossingMinutes();

private:
  static void interpolate(float* series, int size);
};

#endif
