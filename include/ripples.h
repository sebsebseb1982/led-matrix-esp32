#ifndef RIPPLES_H
#define RIPPLES_H

#define MAX_RIPPLES 3

#include "led-panel.h"

struct Ripple {
  int centerX;
  int centerY;
  int radius;
};

class Ripples {
  private:
    LEDPanel *ledPanel;
    Ripple ripples[MAX_RIPPLES];
    int currentRippleIndex; 
    unsigned long lastNewRippleMillis;
    unsigned long nextRippleMillis;
  public:
    Ripples(LEDPanel *ledPanel);
    void setup();
    void loop();
};

#endif