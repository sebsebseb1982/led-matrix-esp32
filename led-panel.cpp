#include "led-panel.h"
#include "colors.h"

LEDPanel::LEDPanel(int toto) {
    // Module configuration
  HUB75_I2S_CFG mxconfig(
    SCREEN_WIDTH,   // module width
    SCREEN_HEIGHT,  // module height
    PANEL_CHAIN     // Chain length
  );

  mxconfig.gpio.e = 32;
  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;
  mxconfig.latch_blanking = 4;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;

  // Display Setup
  this->dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  this->dma_display->begin();
}

void LEDPanel::setup() {
  dma_display->setBrightness8(30);  //0-255
  //dma_display->setBrightness8(255);  //0-255
  dma_display->clearScreen();
  dma_display->fillScreen(Colors::black(dma_display));
}

void LEDPanel::loop() {

}