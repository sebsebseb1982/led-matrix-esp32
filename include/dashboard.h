#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "led-panel.h"

class Dashboard {
  private:
    LEDPanel *ledPanel;
    int tempToY(float temp, float tMin, float tMax);
    void drawFills(const float* etageTemps, const float* extTemps, int n, float tMin, float tMax);
    void drawCurve(const float* temps, int n, float tMin, float tMax, bool isInterior);
    void drawCurrentValues(float etageTemp, float extTemp);
    void drawVentilation(bool isOn);
    void drawSolarEvents(const float* extTemps, float tMin, float tMax);
    void drawCrossingDebug(float tMin, float tMax);
  public:
    Dashboard(LEDPanel *ledPanel);
    void setup();
    void showLoading();
    void loop();
};

#endif
