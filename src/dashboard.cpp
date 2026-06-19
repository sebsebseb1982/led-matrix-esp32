#include "dashboard.h"
#include "data-store.h"
#include "colors.h"
#include "home-assistant.h"
#include <Arduino.h>
#include <stdio.h>

Dashboard::Dashboard(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Dashboard::setup() {
  DataStore::setup();
}

void Dashboard::loop() {
  DataStore::store(
    HomeAssistant::getEntityState("sensor.temperature_etage").toFloat(),
    HomeAssistant::getEntityState("sensor.domo_ext_rieur").toFloat()
  );
  
  this->ledPanel->dma_display->clearScreen();
  this->ledPanel->dma_display->fillScreen(Colors::black(this->ledPanel->dma_display));
  
  drawGrid();
  drawYLabels();
  drawLegend();
  
  int count = DataStore::getCount();
  if (count > 1) {
    int16_t etageTemps[MAX_READINGS];
    int16_t extTemps[MAX_READINGS];
    
    for (int i = 0; i < count; i++) {
      Reading r = DataStore::getReading(i);
      etageTemps[i] = r.tempEtage;
      extTemps[i] = r.tempExt;
    }
    
    drawCurve(etageTemps, count, Colors::blue(this->ledPanel->dma_display), 0, 24);
    drawCurve(extTemps, count, Colors::red(this->ledPanel->dma_display), 0, 24);
  } else {
    this->ledPanel->dma_display->setCursor(5, 30);
    this->ledPanel->dma_display->setTextColor(Colors::lightGrey(this->ledPanel->dma_display));
    this->ledPanel->dma_display->print("Chargement...");
  }
  
  delay(5000);
}

int Dashboard::tempToY(int16_t temp) {
  float ratio = (temp - TEMP_MIN) / (float)TEMP_RANGE;
  return GRAPH_TOP + GRAPH_HEIGHT - (int)(ratio * GRAPH_HEIGHT);
}

int Dashboard::hourToX(int hour) {
  float ratio = hour / 24.0;
  return GRAPH_LEFT + (int)(ratio * GRAPH_WIDTH);
}

void Dashboard::drawGrid() {
  uint16_t gridColor = Colors::darkGrey(this->ledPanel->dma_display);
  
  for (int temp = TEMP_MIN; temp <= TEMP_MAX; temp += 10) {
    int y = tempToY(temp);
    this->ledPanel->dma_display->drawLine(
      GRAPH_LEFT, y, GRAPH_RIGHT, y, gridColor
    );
  }
  
  for (int hour = 0; hour <= 24; hour += 6) {
    int x = hourToX(hour);
    this->ledPanel->dma_display->drawLine(
      x, GRAPH_TOP, x, GRAPH_BOTTOM, gridColor
    );
  }
  
  uint16_t frameColor = Colors::lightGrey(this->ledPanel->dma_display);
  this->ledPanel->dma_display->drawRect(
    GRAPH_LEFT, GRAPH_TOP, GRAPH_WIDTH, GRAPH_HEIGHT, frameColor
  );
}

void Dashboard::drawCurve(const int16_t* temps, int count, uint16_t color, int minHour, int maxHour) {
  int prevX = -1;
  int prevY = -1;
  
  for (int i = 0; i < count; i++) {
    int h = DataStore::getHoursAgo(i);
    if (h < minHour || h > maxHour) continue;
    
    int16_t temp = temps[i];
    if (temp < (TEMP_MIN * 10) || temp > (TEMP_MAX * 10)) continue;
    
    int x = hourToX(h);
    int y = tempToY(temp);
    
    if (prevX >= 0) {
      this->ledPanel->dma_display->drawLine(prevX, prevY, x, y, color);
    }
    
    prevX = x;
    prevY = y;
  }
}

void Dashboard::drawLegend() {
  uint16_t blue = Colors::blue(this->ledPanel->dma_display);
  uint16_t red = Colors::red(this->ledPanel->dma_display);
  uint16_t white = Colors::white(this->ledPanel->dma_display);
  
  this->ledPanel->dma_display->fillRect(44, 1, 4, 3, blue);
  this->ledPanel->dma_display->setCursor(50, 0);
  this->ledPanel->dma_display->setTextColor(white);
  this->ledPanel->dma_display->print("Etage");
  
  this->ledPanel->dma_display->fillRect(44, 5, 4, 3, red);
  this->ledPanel->dma_display->setCursor(50, 4);
  this->ledPanel->dma_display->setTextColor(white);
  this->ledPanel->dma_display->print("Ext");
  
  this->ledPanel->dma_display->setCursor(3, 0);
  this->ledPanel->dma_display->setTextColor(white);
  this->ledPanel->dma_display->print("TEMP 24H");
}

void Dashboard::drawYLabels() {
  uint16_t grey = Colors::darkGrey(this->ledPanel->dma_display);
  
  for (int temp = TEMP_MIN; temp <= TEMP_MAX; temp += 10) {
    int y = tempToY(temp);
    this->ledPanel->dma_display->setCursor(0, y - 1);
    this->ledPanel->dma_display->setTextColor(grey);
    
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", temp);
    this->ledPanel->dma_display->print(buf);
  }
}
