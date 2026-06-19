#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "led-panel.h"

class Dashboard {
  private:
    LEDPanel *ledPanel;
    int tempToY(float temp, float tMin, float tMax);
    void drawFills(const float* etageTemps, const float* extTemps, int numPixels, float tMin, float tMax, uint16_t etageColor, uint16_t extColor);
    void drawCurve(const float* temps, int numPixels, float tMin, float tMax, uint16_t color);
    void drawCurrentValues(float etageTemp, float extTemp);
    void drawVentilation(bool isOn);
  public:
    Dashboard(LEDPanel *ledPanel);
    void setup();
    void loop();
};

#endif
