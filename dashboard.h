#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "led-panel.h"

class Dashboard {
  private:
    LEDPanel *ledPanel;
    void displayTemperature(int x, int y, float value, float min, float max);
    void drawHouse();
    void drawWindow(int x, int y, bool open);
  public:
    Dashboard(LEDPanel *ledPanel);
    void setup();
    void loop();
};

#endif