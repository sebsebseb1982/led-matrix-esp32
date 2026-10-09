#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "led-panel.h"

class Dashboard {
  private:
    LEDPanel *ledPanel;
    // Framebuffer en RAM : la dalle ne se relit pas, or le contour assombrit
    // les pixels deja dessines. Copie sur la dalle en fin de loop().
    GFXcanvas16 *frame;
    int tempToY(float temp, float tMin, float tMax);
    void drawFills(const float* etageTemps, const float* extTemps, int n, float tMin, float tMax);
    void drawCurve(const float* temps, int n, float tMin, float tMax, bool isInterior);
    void drawCurrentValues(float etageTemp, float extTemp);
    void drawVentilation(bool isOn);
    void drawSolarEvents(const float* extTemps, float tMin, float tMax);
  public:
    Dashboard(LEDPanel *ledPanel);
    void showLoading();
    void loop();
};

#endif
