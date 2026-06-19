#include "colors.h"
#include "ripples.h"

Ripples::Ripples(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Ripples::setup() {
  this->currentRippleIndex = 0;
  this->ripples[this->currentRippleIndex] = {
    random(0, SCREEN_WIDTH),
    random(0, SCREEN_HEIGHT),
    1
  };
  this->ripples[1] = {
    random(0, SCREEN_WIDTH),
    random(0, SCREEN_HEIGHT),
    1
  };
  this->ripples[2] = {
    random(0, SCREEN_WIDTH),
    random(0, SCREEN_HEIGHT),
    1
  };
  this->lastNewRippleMillis = millis();
  nextRippleMillis = 1000;
}

void Ripples::loop() {

  if (millis() - lastNewRippleMillis > nextRippleMillis) {
    this->currentRippleIndex = (this->currentRippleIndex + 1) % MAX_RIPPLES;
    this->ripples[currentRippleIndex] = {
      random(0, SCREEN_WIDTH),
      random(0, SCREEN_HEIGHT),
      1
    };

    lastNewRippleMillis = millis();
    nextRippleMillis = random(20, 1000);
  }

  for (int i = 0; i < MAX_RIPPLES; i++) {
    //if (this->ripples[i] != NULL) {
    int blueShade = (((SCREEN_WIDTH - min(this->ripples[i].radius, SCREEN_WIDTH)) * 1.0) / (SCREEN_WIDTH * 1.0)) * 255.0;

    this->ledPanel->dma_display->drawCircle(
      this->ripples[i].centerX,
      this->ripples[i].centerY,
      this->ripples[i].radius,
      Colors::blueShade(this->ledPanel->dma_display, blueShade));

    // Gauche
    if (this->ripples[i].centerX - this->ripples[i].radius < 0) {
      this->ledPanel->dma_display->drawCircle(
        this->ripples[i].centerX * -1,
        this->ripples[i].centerY,
        this->ripples[i].radius,
        Colors::blueShade(this->ledPanel->dma_display, blueShade));
    }

    // Haut
    if (this->ripples[i].centerY - this->ripples[i].radius < 0) {
      this->ledPanel->dma_display->drawCircle(
        this->ripples[i].centerX,
        this->ripples[i].centerY * -1,
        this->ripples[i].radius,
        Colors::blueShade(this->ledPanel->dma_display, blueShade));
    }

    // Droite
    if (this->ripples[i].centerX + this->ripples[i].radius > SCREEN_WIDTH) {
      this->ledPanel->dma_display->drawCircle(
        SCREEN_WIDTH + (SCREEN_WIDTH - this->ripples[i].centerX),
        this->ripples[i].centerY,
        this->ripples[i].radius,
        Colors::blueShade(this->ledPanel->dma_display, blueShade));
    }

    // Bas
    if (this->ripples[i].centerY + this->ripples[i].radius > SCREEN_HEIGHT) {
      this->ledPanel->dma_display->drawCircle(
        this->ripples[i].centerX,
        SCREEN_HEIGHT + (SCREEN_HEIGHT - this->ripples[i].centerY),
        this->ripples[i].radius,
        Colors::blueShade(this->ledPanel->dma_display, blueShade));
    }

    this->ripples[i].radius += 1;
    //}
  }
  this->ledPanel->dma_display->setCursor(20,25);
  this->ledPanel->dma_display->setTextColor(Colors::white(this->ledPanel->dma_display));
  this->ledPanel->dma_display->print("22:36");
  delay(10);
}