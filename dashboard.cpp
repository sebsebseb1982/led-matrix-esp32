#include "colors.h"
#include "dashboard.h"
#include "home-assistant.h"

#define houseWidth 50
#define houseHeight 48
#define roofHeight 10
#define roofOverflow 6
#define pixelsShown 34
#define windowHeight 10
#define windowWidth 3

Dashboard::Dashboard(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Dashboard::setup() {
}

void Dashboard::loop() {
  String temperatureUpstairs = HomeAssistant::getEntityState("sensor.temperature_etage");
  String temperatureDownstairs = HomeAssistant::getEntityState("sensor.temperature_rez_de_chaussee");
  String temperatureOutside = HomeAssistant::getEntityState("sensor.domo_ext_rieur");

  this->ledPanel->dma_display->clearScreen();
  displayTemperature(
    3,
    25,
    temperatureUpstairs.toFloat(),
    18,
    28);

  displayTemperature(
    3,
    48,
    temperatureDownstairs.toFloat(),
    18,
    28);

  displayTemperature(
    38,
    3,
    temperatureOutside.toFloat(),
    0,
    34);

  drawHouse();
  drawWindow(
    pixelsShown - 2,
    SCREEN_HEIGHT - (houseHeight / 4) - (windowHeight / 2) - 1,
    temperatureDownstairs.toFloat() >= temperatureOutside.toFloat());
  drawWindow(
    pixelsShown - 2,
    SCREEN_HEIGHT - ((3 * houseHeight) / 4) - (windowHeight / 2),
    temperatureUpstairs.toFloat() >= temperatureOutside.toFloat());

  delay(5000);
}

void Dashboard::displayTemperature(int x, int y, float value, float minimum, float maximum) {
  float middleBetweenMinAndMax = minimum + ((maximum - minimum) / 2);

  int r, g, b;
  if (value < middleBetweenMinAndMax) {
    float norm_temp = min(float((value - minimum) / (middleBetweenMinAndMax - minimum)), float(1.0));
    r = int(255 * norm_temp);
    g = int(255 * norm_temp);
    b = 255;
  } else if (value > middleBetweenMinAndMax) {
    float norm_temp = max(float(1.0 - (value - middleBetweenMinAndMax) / (maximum - middleBetweenMinAndMax)), float(0.0));
    r = 255;
    g = int(255 * norm_temp);
    b = int(255 * norm_temp);
  } else {
    r = 255;
    g = 255;
    b = 255;
  }

  this->ledPanel->dma_display->setCursor(x, y);
  this->ledPanel->dma_display->setTextColor(Colors::rgb(
    this->ledPanel->dma_display,
    r,
    g,
    b));
  this->ledPanel->dma_display->print(String(value, 1));
}

void Dashboard::drawHouse() {
  uint16_t wallsColor = Colors::rgb(this->ledPanel->dma_display, 168, 168, 168);
  uint16_t tilesColor = Colors::rgb(this->ledPanel->dma_display, 255, 200, 133);

  this->ledPanel->dma_display->drawRect(
    pixelsShown - houseWidth,
    SCREEN_HEIGHT - houseHeight,
    houseWidth,
    houseHeight,
    wallsColor);

  this->ledPanel->dma_display->drawLine(
    pixelsShown - houseWidth,
    SCREEN_HEIGHT - (houseHeight / 2),
    pixelsShown - 1,
    SCREEN_HEIGHT - (houseHeight / 2),
    wallsColor);

  this->ledPanel->dma_display->drawTriangle(
    pixelsShown - houseWidth - roofOverflow,
    SCREEN_HEIGHT - houseHeight,
    pixelsShown + roofOverflow,
    SCREEN_HEIGHT - houseHeight,
    (pixelsShown - houseWidth) + (houseWidth / 2),
    SCREEN_HEIGHT - houseHeight - roofHeight,
    wallsColor);
}

void Dashboard::drawWindow(int x, int y, bool open) {

  this->ledPanel->dma_display->fillRect(
    x,
    y,
    windowWidth,
    windowHeight,
    Colors::black(this->ledPanel->dma_display));

  uint16_t wallsColor = Colors::rgb(this->ledPanel->dma_display, 168, 168, 168);
  uint16_t blueColor = Colors::blue(this->ledPanel->dma_display);
  uint16_t redColor = Colors::red(this->ledPanel->dma_display);

  this->ledPanel->dma_display->drawLine(
    x,
    y,
    x + windowWidth - 1,
    y,
    wallsColor);

  this->ledPanel->dma_display->drawLine(
    x,
    y + windowHeight,
    x + windowWidth - 1,
    y + windowHeight,
    wallsColor);

  if (open) {
    for (int i = 0; i < 2; i++) {
      this->ledPanel->dma_display->drawPixel(
        x - 1,
        y + 3 + i * 4,
        blueColor);

      this->ledPanel->dma_display->drawPixel(
        x,
        y + 2 + i * 4,
        blueColor);

      this->ledPanel->dma_display->drawPixel(
        x + 1,
        y + 3 + i * 4,
        blueColor);

      this->ledPanel->dma_display->drawPixel(
        x + 2,
        y + 4 + i * 4,
        blueColor);

      this->ledPanel->dma_display->drawPixel(
        x + 3,
        y + 3 + i * 4,
        blueColor);
    }
  } else {
    this->ledPanel->dma_display->drawCircle(
      x + 1,
      y + 5,
      3,
      redColor);

    this->ledPanel->dma_display->drawLine(
      x - 1,
      y + 7,
      x + 3,
      y + 3,
      redColor);
  }
}
