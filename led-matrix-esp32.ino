// Example sketch which shows how to display some patterns
// on a 64x32 LED matrix
//

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "wifi-connection.h"
#include "ota.h"

#define SCREEN_WIDTH 64   // Number of pixels wide of each INDIVIDUAL panel module.
#define SCREEN_HEIGHT 64  // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1     // Total number of panels chained one to another

//MatrixPanel_I2S_DMA dma_display;
MatrixPanel_I2S_DMA* dma_display = nullptr;

uint16_t myBLACK = dma_display->color565(0, 0, 0);
uint16_t myGREY = dma_display->color565(50, 50, 50);
uint16_t myGREY2 = dma_display->color565(100, 100, 100);
uint16_t myWHITE = dma_display->color565(255, 255, 255);
uint16_t myRED = dma_display->color565(255, 0, 0);
uint16_t myGREEN = dma_display->color565(0, 255, 0);
uint16_t myBLUE = dma_display->color565(0, 0, 255);

#define HOURS_HAND_LENGTH 6
#define MINUTES_HAND_LENGTH 18
#define SECONDS_HAND_LENGTH 26

int hour = 0;
int minutes = 26;
int seconds = 43;

void setup() {
  Serial.begin(115200);
  Serial.println("setup()");
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
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  //dma_display->setBrightness8(30);  //0-255
  dma_display->setBrightness8(255);  //0-255
  dma_display->clearScreen();

  // fix the screen with green
  dma_display->fillScreen(myBLACK);
  WiFiConnection::setup();
  OTA::setup();
}

void loop() {
  WiFiConnection::loop();
  OTA::loop();

  dma_display->clearScreen();
  int clockCenterX = SCREEN_WIDTH / 2 - 1;
  int clockCenterY = SCREEN_HEIGHT / 2 - 1;
  int clockRadius = SCREEN_HEIGHT / 2 - 1;

  dma_display->drawCircle(
    clockCenterX,
    clockCenterY,
    clockRadius,
    myGREY2);

  for (int hour = 0; hour < 12; hour++) {
    int indexXStart = clockCenterX + round((clockRadius - 4) * cos(((hour * 1.0 / 12) * 360) * M_PI / 180));
    int indexYStart = clockCenterY + round((clockRadius - 4) * sin(((hour * 1.0 / 12) * 360) * M_PI / 180));
    int indexXEnd = clockCenterX + round((clockRadius - 2) * cos(((hour * 1.0 / 12) * 360) * M_PI / 180));
    int indexYEnd = clockCenterY + round((clockRadius - 2) * sin(((hour * 1.0 / 12) * 360) * M_PI / 180));

    dma_display->drawLine(
      indexXStart,
      indexYStart,
      indexXEnd,
      indexYEnd,
      myWHITE);
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

  dma_display->drawLine(
    clockCenterX,
    clockCenterY,
    hourX,
    SCREEN_HEIGHT - hourY,
    myWHITE);
}

void drawMinutesHand(int clockCenterX, int clockCenterY) {
  int minutesHandX = clockCenterX + round(MINUTES_HAND_LENGTH * cos((90 - ((minutes * 1.0 / 60)) * 360) * M_PI / 180));
  int minutesHandY = clockCenterY + round(MINUTES_HAND_LENGTH * sin((90 - ((minutes * 1.0 / 60)) * 360) * M_PI / 180));

  dma_display->drawLine(
    clockCenterX,
    clockCenterY,
    minutesHandX,
    SCREEN_HEIGHT - minutesHandY,
    myWHITE);
}

void drawSecondsHand(int clockCenterX, int clockCenterY) {
  int secondsHandX = clockCenterX + round(SECONDS_HAND_LENGTH * cos((90 - ((seconds * 1.0 / 60)) * 360) * M_PI / 180));
  int secondsHandY = clockCenterY + round(SECONDS_HAND_LENGTH * sin((90 - ((seconds * 1.0 / 60)) * 360) * M_PI / 180));
  int secondsHandX2 = clockCenterX + round((SECONDS_HAND_LENGTH - 5) * cos((90 - ((seconds * 1.0 / 60)) * 360) * M_PI / 180));
  int secondsHandY2 = clockCenterY + round((SECONDS_HAND_LENGTH - 5) * sin((90 - ((seconds * 1.0 / 60)) * 360) * M_PI / 180));

  dma_display->drawLine(
    secondsHandX2,
    SCREEN_HEIGHT - secondsHandY2,
    secondsHandX,
    SCREEN_HEIGHT - secondsHandY,
    myGREEN);

  dma_display->drawLine(
    clockCenterX,
    clockCenterY,
    secondsHandX2,
    SCREEN_HEIGHT - secondsHandY2,
    myWHITE);
}