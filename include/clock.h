#ifndef CLOCK_H
#define CLOCK_H

#include "led-panel.h"

#define HOURS_HAND_LENGTH 6
#define MINUTES_HAND_LENGTH 18
#define SECONDS_HAND_LENGTH 26

class Clock {
  private:
    int hour;
    int minutes;
    int seconds;
    LEDPanel *ledPanel;
    void drawHoursHand(int clockCenterX, int clockCenterY);
    void drawMinutesHand(int clockCenterX, int clockCenterY);
    void drawSecondsHand(int clockCenterX, int clockCenterY);
  public:
    Clock(LEDPanel *ledPanel);
    void setup();
    void loop();
};

#endif