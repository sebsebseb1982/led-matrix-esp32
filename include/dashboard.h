#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "led-panel.h"

class Dashboard {
  private:
    LEDPanel *ledPanel;
    int tempToY(int16_t temp);
    int heureToX(uint8_t heure);
    void drawCurve(const int16_t* temps, int count, const uint8_t* heures, uint16_t color);
  public:
    Dashboard(LEDPanel *ledPanel);
    void setup();
    void loop();
};

#endif
