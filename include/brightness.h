#ifndef BRIGHTNESS_H
#define BRIGHTNESS_H
#include "led-panel.h"

#define LIGHT_SENSOR_PIN 34

class Brightness {
  private:
    LEDPanel *ledPanel;
  public:
    Brightness(LEDPanel *ledPanel);
    void setup();
    void loop();
};

#endif