#include "dashboard.h"
#include "colors.h"
#include "home-assistant.h"
#include "data-store.h"
#include <Arduino.h>
#include <time.h>

#define TEMP_MIN -10
#define TEMP_MAX 40
#define TEMP_RANGE (TEMP_MAX - TEMP_MIN)
#define MAX_POINTS 300

int Dashboard::tempToY(int16_t temp) {
  float ratio = (temp - TEMP_MIN) / (float)TEMP_RANGE;
  return 63 - (int)(ratio * 63);
}

void Dashboard::drawCurve(const int16_t* temps, int count, const uint8_t* heures, uint16_t color) {
  for (int i = 0; i < count; i++) {
    int16_t temp = temps[i];
    if (temp < (TEMP_MIN * 10) || temp > (TEMP_MAX * 10)) continue;
    int x = heures[i];
    int y = tempToY(temp);
    if (i > 0 && temps[i-1] >= (TEMP_MIN * 10) && temps[i-1] <= (TEMP_MAX * 10)) {
      int prevX = heures[i-1];
      int prevY = tempToY(temps[i-1]);
      this->ledPanel->dma_display->drawLine(prevX, prevY, x, y, color);
    }
  }
}

Dashboard::Dashboard(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Dashboard::setup() {
}

void Dashboard::loop() {
  struct tm timeinfo;
  int retries = 0;
  while (!getLocalTime(&timeinfo) && retries < 10) {
    delay(500);
    retries++;
  }

  this->ledPanel->dma_display->clearScreen();
  this->ledPanel->dma_display->fillScreen(Colors::black(this->ledPanel->dma_display));

  HistoryPoint etagePoints[MAX_POINTS];
  int etageCount = HomeAssistant::getHistory("sensor.temperature_etage", etagePoints, MAX_POINTS, 24);

  HistoryPoint extPoints[MAX_POINTS];
  int extCount = HomeAssistant::getHistory("sensor.domo_ext_rieur", extPoints, MAX_POINTS, 24);

  if (etageCount > 0 && extCount > 0) {
    DataStore::store(etagePoints[etageCount - 1].value, extPoints[extCount - 1].value);
  }

  if (etageCount > 1 && extCount > 1) {
    long minTs = etagePoints[0].ts;
    for (int i = 1; i < etageCount; i++) {
      if (etagePoints[i].ts < minTs) minTs = etagePoints[i].ts;
    }
    for (int i = 0; i < extCount; i++) {
      if (extPoints[i].ts < minTs) minTs = extPoints[i].ts;
    }

    long maxTs = etagePoints[0].ts;
    for (int i = 1; i < etageCount; i++) {
      if (etagePoints[i].ts > maxTs) maxTs = etagePoints[i].ts;
    }
    for (int i = 0; i < extCount; i++) {
      if (extPoints[i].ts > maxTs) maxTs = extPoints[i].ts;
    }

    long timeRange = maxTs - minTs;
    if (timeRange == 0) timeRange = 1;

    uint8_t etageXs[MAX_POINTS];
    int16_t etageTemps[MAX_POINTS];

    for (int i = 0; i < etageCount; i++) {
      int x = (int)(((etagePoints[i].ts - minTs) / (float)timeRange) * 63);
      if (x < 0) x = 0;
      if (x > 63) x = 63;
      etageXs[i] = (uint8_t)x;
      etageTemps[i] = (int16_t)(etagePoints[i].value * 10);
    }

    drawCurve(etageTemps, etageCount, etageXs, Colors::blue(this->ledPanel->dma_display));

    uint8_t extXs[MAX_POINTS];
    int16_t extTemps[MAX_POINTS];

    for (int i = 0; i < extCount; i++) {
      int x = (int)(((extPoints[i].ts - minTs) / (float)timeRange) * 63);
      if (x < 0) x = 0;
      if (x > 63) x = 63;
      extXs[i] = (uint8_t)x;
      extTemps[i] = (int16_t)(extPoints[i].value * 10);
    }

    drawCurve(extTemps, extCount, extXs, Colors::red(this->ledPanel->dma_display));
  } else {
    this->ledPanel->dma_display->setCursor(5, 30);
    this->ledPanel->dma_display->setTextColor(Colors::lightGrey(this->ledPanel->dma_display));
    this->ledPanel->dma_display->print("Pas de donnees");
  }

  delay(5000);
}
