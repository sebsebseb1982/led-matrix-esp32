#include "dashboard.h"
#include "data-store.h"
#include "colors.h"
#include "home-assistant.h"
#include <Arduino.h>
#include <WiFiUdp.h>
#include <time.h>

#define TEMP_MIN -10
#define TEMP_MAX 40
#define TEMP_RANGE (TEMP_MAX - TEMP_MIN)

WiFiUDP udp;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

void getNTPTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

uint8_t getCurrentHour() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0;
  }
  return timeinfo.tm_hour;
}

Dashboard::Dashboard(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Dashboard::setup() {
  DataStore::setup();
  getNTPTime();
}

void Dashboard::loop() {
  getNTPTime();
  delay(1000);
  
  float tempEtage = HomeAssistant::getEntityState("sensor.temperature_etage").toFloat();
  float tempExt = HomeAssistant::getEntityState("sensor.domo_ext_rieur").toFloat();
  
  uint8_t heure = getCurrentHour();
  DataStore::store(tempEtage, tempExt, heure);
  
  this->ledPanel->dma_display->clearScreen();
  this->ledPanel->dma_display->fillScreen(Colors::black(this->ledPanel->dma_display));
  
  int count = DataStore::getCount();
  if (count > 1) {
    int16_t etageTemps[MAX_READINGS];
    int16_t extTemps[MAX_READINGS];
    uint8_t heures[MAX_READINGS];
    
    for (int i = 0; i < count; i++) {
      Reading r = DataStore::getReading(i);
      etageTemps[i] = r.tempEtage;
      extTemps[i] = r.tempExt;
      heures[i] = r.heure;
    }
    
    drawCurve(etageTemps, count, heures, Colors::blue(this->ledPanel->dma_display));
    drawCurve(extTemps, count, heures, Colors::red(this->ledPanel->dma_display));
  } else {
    this->ledPanel->dma_display->setCursor(5, 30);
    this->ledPanel->dma_display->setTextColor(Colors::lightGrey(this->ledPanel->dma_display));
    this->ledPanel->dma_display->print("Chargement...");
  }
  
  delay(5000);
}

int Dashboard::tempToY(int16_t temp) {
  float ratio = (temp - TEMP_MIN) / (float)TEMP_RANGE;
  return 63 - (int)(ratio * 63);
}

int Dashboard::heureToX(uint8_t heure) {
  return (int)((heure / 24.0) * 63);
}

void Dashboard::drawCurve(const int16_t* temps, int count, const uint8_t* heures, uint16_t color) {
  int prevX = -1;
  int prevY = -1;
  
  for (int i = 0; i < count; i++) {
    int16_t temp = temps[i];
    if (temp < (TEMP_MIN * 10) || temp > (TEMP_MAX * 10)) continue;
    
    int x = heureToX(heures[i]);
    int y = tempToY(temp);
    
    if (prevX >= 0) {
      this->ledPanel->dma_display->drawLine(prevX, prevY, x, y, color);
    }
    
    prevX = x;
    prevY = y;
  }
}
