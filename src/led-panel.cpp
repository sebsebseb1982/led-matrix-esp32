#include "led-panel.h"
#include "colors.h"

LEDPanel::LEDPanel() {
  // Module configuration
  HUB75_I2S_CFG mxconfig(
    SCREEN_WIDTH,   // module width
    SCREEN_HEIGHT,  // module height
    PANEL_CHAIN     // Chain length
  );

  mxconfig.gpio.e = 32;
  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;

  this->dma_display = new MatrixPanel_I2S_DMA(mxconfig);
}

void LEDPanel::setup() {
  // begin() alloue les buffers DMA : il ne doit pas etre appele depuis le
  // constructeur d'un objet global, qui s'execute avant le demarrage de
  // FreeRTOS (sinon plus assez de RAM interne pour la pile de la tache idle).
  dma_display->begin();
  dma_display->setBrightness8(10);  //0-255
  dma_display->clearScreen();
  dma_display->fillScreen(Colors::BLACK);
}
