#ifndef BITMAP_H
#define BITMAP_H
//#define HEADER_BYTES_SIZE 122
#define HEADER_BYTES_SIZE 54

#include "led-panel.h"

class Bitmap {
  private:
    LEDPanel *ledPanel;
    void downloadAndDisplayImage();
  public:
    Bitmap(LEDPanel *ledPanel);
    void setup();
    void loop();
};

#endif