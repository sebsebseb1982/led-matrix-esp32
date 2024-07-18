#include "colors.h"
#include "clock.h"

Clock::Clock(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Clock::setup() {
}

void Clock::loop() {
  int clockCenterX = SCREEN_WIDTH / 2 - 1;
  int clockCenterY = SCREEN_HEIGHT / 2 - 1;
  int clockRadius = SCREEN_HEIGHT / 2 - 1;

  this->ledPanel->dma_display->drawCircle(
    clockCenterX,
    clockCenterY,
    clockRadius,
    Colors::grey(this->ledPanel->dma_display));

  for (int hourIndex = 0; hourIndex < 12; hourIndex++) {
    int indexXStart = clockCenterX + round((clockRadius - 4) * cos(((hourIndex * 1.0 / 12) * 360) * M_PI / 180));
    int indexYStart = clockCenterY + round((clockRadius - 4) * sin(((hourIndex * 1.0 / 12) * 360) * M_PI / 180));
    int indexXEnd = clockCenterX + round((clockRadius - 2) * cos(((hourIndex * 1.0 / 12) * 360) * M_PI / 180));
    int indexYEnd = clockCenterY + round((clockRadius - 2) * sin(((hourIndex * 1.0 / 12) * 360) * M_PI / 180));

    this->ledPanel->dma_display->drawLine(
      indexXStart,
      indexYStart,
      indexXEnd,
      indexYEnd,
      Colors::white(this->ledPanel->dma_display));
  }

  drawHoursHand(clockCenterX, clockCenterY);
  drawMinutesHand(clockCenterX, clockCenterY);
  drawSecondsHand(clockCenterX, clockCenterY);

  delay(60);
  seconds++;
}

void Clock::drawHoursHand(int clockCenterX, int clockCenterY) {
  int hourX = clockCenterX + round(HOURS_HAND_LENGTH * cos((90 - ((this->hour * 1.0 / 12) * 360)) * M_PI / 180));
  int hourY = clockCenterY + round(HOURS_HAND_LENGTH * sin((90 - ((this->hour * 1.0 / 12) * 360)) * M_PI / 180));

  this->ledPanel->dma_display->drawLine(
    clockCenterX,
    clockCenterY,
    hourX,
    SCREEN_HEIGHT - hourY,
    Colors::white(this->ledPanel->dma_display));
}

void Clock::drawMinutesHand(int clockCenterX, int clockCenterY) {
  int minutesHandX = clockCenterX + round(MINUTES_HAND_LENGTH * cos((90 - ((this->minutes * 1.0 / 60)) * 360) * M_PI / 180));
  int minutesHandY = clockCenterY + round(MINUTES_HAND_LENGTH * sin((90 - ((this->minutes * 1.0 / 60)) * 360) * M_PI / 180));

  this->ledPanel->dma_display->drawLine(
    clockCenterX,
    clockCenterY,
    minutesHandX,
    SCREEN_HEIGHT - minutesHandY,
    Colors::white(this->ledPanel->dma_display));
}

void Clock::drawSecondsHand(int clockCenterX, int clockCenterY) {
  int secondsHandX = clockCenterX + round(SECONDS_HAND_LENGTH * cos((90 - ((this->seconds * 1.0 / 60)) * 360) * M_PI / 180));
  int secondsHandY = clockCenterY + round(SECONDS_HAND_LENGTH * sin((90 - ((this->seconds * 1.0 / 60)) * 360) * M_PI / 180));
  int secondsHandX2 = clockCenterX + round((SECONDS_HAND_LENGTH - 5) * cos((90 - ((this->seconds * 1.0 / 60)) * 360) * M_PI / 180));
  int secondsHandY2 = clockCenterY + round((SECONDS_HAND_LENGTH - 5) * sin((90 - ((this->seconds * 1.0 / 60)) * 360) * M_PI / 180));

  this->ledPanel->dma_display->drawLine(
    secondsHandX2,
    SCREEN_HEIGHT - secondsHandY2,
    secondsHandX,
    SCREEN_HEIGHT - secondsHandY,
    Colors::red(this->ledPanel->dma_display));

  this->ledPanel->dma_display->drawLine(
    clockCenterX,
    clockCenterY,
    secondsHandX2,
    SCREEN_HEIGHT - secondsHandY2,
    Colors::white(this->ledPanel->dma_display));
}