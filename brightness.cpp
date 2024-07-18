#include "brightness.h"
#include "colors.h"

Brightness::Brightness(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Brightness::setup() {  
}

void Brightness::loop() {  
  float luminosity = analogRead(LIGHT_SENSOR_PIN) / 4095.0;
  int brightness = max(int(luminosity*255.0), 20);

  this->ledPanel->dma_display->setCursor(1, 50);
  this->ledPanel->dma_display->setTextColor(Colors::black(this->ledPanel->dma_display));
  this->ledPanel->dma_display->print(String(brightness));

  this->ledPanel->dma_display->setBrightness8(brightness);
}