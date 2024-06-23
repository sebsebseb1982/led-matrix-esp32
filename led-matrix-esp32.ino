#include "wifi-connection.h"
#include "ota.h"
#include "led-panel.h"
#include "colors.h"

#define HOURS_HAND_LENGTH 6
#define MINUTES_HAND_LENGTH 18
#define SECONDS_HAND_LENGTH 26

int hour = 0;
int minutes = 26;
int seconds = 43;

LEDPanel ledPanel(8);

void setup() {
  Serial.begin(115200);
  Serial.println("setup()");

  WiFiConnection::setup();
  OTA::setup();
  ledPanel.setup();
}

void loop() {
  WiFiConnection::loop();
  OTA::loop();
  ledPanel.loop();

  ledPanel.dma_display->clearScreen();
  int clockCenterX = SCREEN_WIDTH / 2 - 1;
  int clockCenterY = SCREEN_HEIGHT / 2 - 1;
  int clockRadius = SCREEN_HEIGHT / 2 - 1;

  ledPanel.dma_display->drawCircle(
    clockCenterX,
    clockCenterY,
    clockRadius,
    Colors::grey(ledPanel.dma_display));

  for (int hour = 0; hour < 12; hour++) {
    int indexXStart = clockCenterX + round((clockRadius - 4) * cos(((hour * 1.0 / 12) * 360) * M_PI / 180));
    int indexYStart = clockCenterY + round((clockRadius - 4) * sin(((hour * 1.0 / 12) * 360) * M_PI / 180));
    int indexXEnd = clockCenterX + round((clockRadius - 2) * cos(((hour * 1.0 / 12) * 360) * M_PI / 180));
    int indexYEnd = clockCenterY + round((clockRadius - 2) * sin(((hour * 1.0 / 12) * 360) * M_PI / 180));

    ledPanel.dma_display->drawLine(
      indexXStart,
      indexYStart,
      indexXEnd,
      indexYEnd,
      Colors::white(ledPanel.dma_display));
  }

  drawHoursHand(clockCenterX, clockCenterY);
  drawMinutesHand(clockCenterX, clockCenterY);
  drawSecondsHand(clockCenterX, clockCenterY);

  delay(1000);
  seconds++;
}

void drawHoursHand(int clockCenterX, int clockCenterY) {
  int hourX = clockCenterX + round(HOURS_HAND_LENGTH * cos((90 - ((hour * 1.0 / 12) * 360)) * M_PI / 180));
  int hourY = clockCenterY + round(HOURS_HAND_LENGTH * sin((90 - ((hour * 1.0 / 12) * 360)) * M_PI / 180));

  ledPanel.dma_display->drawLine(
    clockCenterX,
    clockCenterY,
    hourX,
    SCREEN_HEIGHT - hourY,
    Colors::white(ledPanel.dma_display));
}

void drawMinutesHand(int clockCenterX, int clockCenterY) {
  int minutesHandX = clockCenterX + round(MINUTES_HAND_LENGTH * cos((90 - ((minutes * 1.0 / 60)) * 360) * M_PI / 180));
  int minutesHandY = clockCenterY + round(MINUTES_HAND_LENGTH * sin((90 - ((minutes * 1.0 / 60)) * 360) * M_PI / 180));

  ledPanel.dma_display->drawLine(
    clockCenterX,
    clockCenterY,
    minutesHandX,
    SCREEN_HEIGHT - minutesHandY,
    Colors::white(ledPanel.dma_display));
}

void drawSecondsHand(int clockCenterX, int clockCenterY) {
  int secondsHandX = clockCenterX + round(SECONDS_HAND_LENGTH * cos((90 - ((seconds * 1.0 / 60)) * 360) * M_PI / 180));
  int secondsHandY = clockCenterY + round(SECONDS_HAND_LENGTH * sin((90 - ((seconds * 1.0 / 60)) * 360) * M_PI / 180));
  int secondsHandX2 = clockCenterX + round((SECONDS_HAND_LENGTH - 5) * cos((90 - ((seconds * 1.0 / 60)) * 360) * M_PI / 180));
  int secondsHandY2 = clockCenterY + round((SECONDS_HAND_LENGTH - 5) * sin((90 - ((seconds * 1.0 / 60)) * 360) * M_PI / 180));

  ledPanel.dma_display->drawLine(
    secondsHandX2,
    SCREEN_HEIGHT - secondsHandY2,
    secondsHandX,
    SCREEN_HEIGHT - secondsHandY,
    Colors::red(ledPanel.dma_display));

  ledPanel.dma_display->drawLine(
    clockCenterX,
    clockCenterY,
    secondsHandX2,
    SCREEN_HEIGHT - secondsHandY2,
    Colors::white(ledPanel.dma_display));
}