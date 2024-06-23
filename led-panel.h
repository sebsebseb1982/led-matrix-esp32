#ifndef LED_PANEL_H
#define LED_PANEL_H

#define SCREEN_WIDTH 64   // Number of pixels wide of each INDIVIDUAL panel module.
#define SCREEN_HEIGHT 64  // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1     // Total number of panels chained one to another

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

class LEDPanel {
  public:
    MatrixPanel_I2S_DMA* dma_display;
    LEDPanel(int toto);
    void setup();
    void loop();
};

#endif