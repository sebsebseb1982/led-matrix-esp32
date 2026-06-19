#include "dashboard.h"
#include "colors.h"
#include "home-assistant.h"
#include "data-store.h"
#include "buzzer.h"
#include <esp_attr.h>

RTC_DATA_ATTR static int lastVentilationState = -1; // survit au deep sleep
#include <Arduino.h>
#include <time.h>
#include <math.h>

#define PANEL_W     64
#define PANEL_H     64
#define CURVE_PAD    4

int Dashboard::tempToY(float temp, float tMin, float tMax) {
  float range = tMax - tMin;
  float ratio = (range == 0.0f) ? 0.5f : (temp - tMin) / range;
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;
  int topY    = CURVE_PAD;
  int bottomY = PANEL_H - 1 - CURVE_PAD;
  return bottomY - (int)(ratio * (bottomY - topY));
}

static uint16_t dimColor565(uint16_t c, int div) {
  uint8_t r = ((c >> 11) & 0x1F) / div;
  uint8_t g = ((c >> 5)  & 0x3F) / div;
  uint8_t b = (c          & 0x1F) / div;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static uint16_t addColors565(uint16_t c1, uint16_t c2) {
  uint8_t r = min(31, ((c1 >> 11) & 0x1F) + ((c2 >> 11) & 0x1F));
  uint8_t g = min(63, ((c1 >> 5)  & 0x3F) + ((c2 >> 5)  & 0x3F));
  uint8_t b = min(31, (c1         & 0x1F) + (c2         & 0x1F));
  return (uint16_t)((r << 11) | (g << 5) | b);
}

void Dashboard::drawFills(const float* etageTemps, const float* extTemps,
                           int numPixels, float tMin, float tMax,
                           uint16_t etageColor, uint16_t extColor) {
  uint16_t dimEtage = dimColor565(etageColor, 7);
  uint16_t dimExt   = dimColor565(extColor,   7);
  int bottomY = PANEL_H - 1;

  for (int x = 0; x < numPixels; x++) {
    bool hasEtage = !isnan(etageTemps[x]);
    bool hasExt   = !isnan(extTemps[x]);
    int yEtage = hasEtage ? tempToY(etageTemps[x], tMin, tMax) : bottomY + 1;
    int yExt   = hasExt   ? tempToY(extTemps[x],   tMin, tMax) : bottomY + 1;

    for (int y = CURVE_PAD; y <= bottomY; y++) {
      bool inEtage = hasEtage && y > yEtage;
      bool inExt   = hasExt   && y > yExt;
      if      (inEtage && inExt) this->ledPanel->dma_display->drawPixel(x, y, addColors565(dimEtage, dimExt));
      else if (inEtage)           this->ledPanel->dma_display->drawPixel(x, y, dimEtage);
      else if (inExt)             this->ledPanel->dma_display->drawPixel(x, y, dimExt);
    }
  }
}

void Dashboard::drawCurve(const float* temps, int numPixels, float tMin, float tMax, uint16_t color) {
  for (int x = 1; x < numPixels; x++) {
    if (isnan(temps[x]) || isnan(temps[x - 1])) continue;
    int y0 = tempToY(temps[x - 1], tMin, tMax);
    int y1 = tempToY(temps[x], tMin, tMax);
    this->ledPanel->dma_display->drawLine(x - 1, y0, x, y1, color);
  }
}

// Coche verte 5×5
static void drawCheckmark(MatrixPanel_I2S_DMA* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+4, y+0, color);
  disp->drawPixel(x+3, y+1, color);
  disp->drawPixel(x+2, y+2, color); disp->drawPixel(x+1, y+2, color);
  disp->drawPixel(x+0, y+3, color);
}

// Croix rouge 5×5
static void drawCross(MatrixPanel_I2S_DMA* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+0, y+0, color); disp->drawPixel(x+4, y+0, color);
  disp->drawPixel(x+1, y+1, color); disp->drawPixel(x+3, y+1, color);
  disp->drawPixel(x+2, y+2, color);
  disp->drawPixel(x+1, y+3, color); disp->drawPixel(x+3, y+3, color);
  disp->drawPixel(x+0, y+4, color); disp->drawPixel(x+4, y+4, color);
}

// °C en pixel art : ° (3x3) + gap (1px) + C (3x5) = 7px wide, 5px tall
static void drawDegC(MatrixPanel_I2S_DMA* disp, int x, int y, uint16_t color) {
  // °
  disp->drawPixel(x+1, y+0, color);
  disp->drawPixel(x+0, y+1, color);
  disp->drawPixel(x+2, y+1, color);
  disp->drawPixel(x+1, y+2, color);
  // C
  disp->drawPixel(x+5, y+0, color); disp->drawPixel(x+6, y+0, color);
  disp->drawPixel(x+4, y+1, color);
  disp->drawPixel(x+4, y+2, color);
  disp->drawPixel(x+4, y+3, color);
  disp->drawPixel(x+5, y+4, color); disp->drawPixel(x+6, y+4, color);
}

void Dashboard::drawCurrentValues(float etageTemp, float extTemp) {
  bool hasEtage = !isnan(etageTemp);
  bool hasExt   = !isnan(extTemp);
  if (!hasEtage && !hasExt) return;

  float highTemp;
  uint16_t highColor;
  if (hasEtage && hasExt) {
    bool etageIsHigh = etageTemp >= extTemp;
    highTemp  = etageIsHigh ? etageTemp : extTemp;
    highColor = etageIsHigh ? Colors::blue(this->ledPanel->dma_display)
                            : Colors::red(this->ledPanel->dma_display);
  } else if (hasEtage) {
    highTemp  = etageTemp;
    highColor = Colors::blue(this->ledPanel->dma_display);
  } else {
    highTemp  = extTemp;
    highColor = Colors::red(this->ledPanel->dma_display);
  }

  char buf[8];
  sprintf(buf, "%.1f", highTemp);
  for (int i = 0; buf[i]; i++) if (buf[i] == '.') { buf[i] = ','; break; }

  const int degCW    = 7;  // largeur du symbole °C pixel art
  const int gap      = 0;
  const int rightPad = 1;
  int textW  = strlen(buf) * 6;
  int totalW = textW + gap + degCW + rightPad;
  int textX  = PANEL_W - totalW;

  this->ledPanel->dma_display->setTextSize(1);
  this->ledPanel->dma_display->setTextWrap(false);
  this->ledPanel->dma_display->fillRect(textX, 1, totalW, 8, Colors::black(this->ledPanel->dma_display));
  this->ledPanel->dma_display->setCursor(textX, 1);
  this->ledPanel->dma_display->setTextColor(highColor);
  this->ledPanel->dma_display->print(buf);
  drawDegC(this->ledPanel->dma_display, textX + textW + gap, 1, highColor);
}

void Dashboard::drawVentilation(bool isOn) {
  auto* disp = this->ledPanel->dma_display;
  const int margin = 1;
  const int pad    = 1;
  const int symSz  = 5;
  int rx = margin;
  int ry = PANEL_H - margin - (2 * pad + symSz);  // ancrée en bas à gauche
  disp->fillRect(rx, ry, 2 * pad + symSz, 2 * pad + symSz, Colors::rgb(disp, 150, 150, 150));
  if (isOn)
    drawCheckmark(disp, rx + pad, ry + pad, Colors::green(disp));
  else
    drawCross(disp, rx + pad, ry + pad, Colors::red(disp));
}

Dashboard::Dashboard(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Dashboard::setup() {
  Serial.println("[dash] setup OK");
}

void Dashboard::loop() {
  Serial.println("[dash] === BOUCLE ===");

  struct tm timeinfo;
  int retries = 0;
  while (!getLocalTime(&timeinfo) && retries < 10) {
    delay(500);
    retries++;
  }

  if (retries >= 10) {
    Serial.println("[dash] ERREUR: getLocalTime echoue");
  } else {
    Serial.printf("[dash] NTP OK: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }

  static float etageTemps[PANEL_W];
  static float extTemps[PANEL_W];

  Serial.println("[dash] fetch temperature_etage...");
  int etageValid = HomeAssistant::getHistory("sensor.temperature_etage", etageTemps, PANEL_W, 24);
  Serial.printf("[dash] etage: %d pixels valides\n", etageValid);

  Serial.println("[dash] fetch domo_ext_rieur...");
  int extValid = HomeAssistant::getHistory("sensor.domo_ext_rieur", extTemps, PANEL_W, 24);
  Serial.printf("[dash] ext: %d pixels valides\n", extValid);

  Serial.println("[dash] fetch etat_ventilation...");
  String ventStr = HomeAssistant::getEntityState("input_boolean.etat_ventilation");
  Serial.printf("[dash] ventilation: %s\n", ventStr.c_str());
  bool ventIsOn = (ventStr == "on");
  if (lastVentilationState != -1 && (bool)lastVentilationState != ventIsOn) {
    Buzzer::beepbeepbeep(ventIsOn ? 50 : 200);
  }
  lastVentilationState = ventIsOn ? 1 : 0;

  this->ledPanel->dma_display->clearScreen();
  this->ledPanel->dma_display->fillScreen(Colors::black(this->ledPanel->dma_display));

  // Stocker la derniere valeur connue pour chaque capteur
  float lastEtage = NAN, lastExt = NAN;
  for (int i = PANEL_W - 1; i >= 0; i--) {
    if (isnan(lastEtage) && !isnan(etageTemps[i])) lastEtage = etageTemps[i];
    if (isnan(lastExt)   && !isnan(extTemps[i]))   lastExt   = extTemps[i];
    if (!isnan(lastEtage) && !isnan(lastExt)) break;
  }
  if (!isnan(lastEtage) && !isnan(lastExt)) {
    DataStore::store(lastEtage, lastExt);
    Serial.printf("[dash] DataStore: etage=%.1f ext=%.1f\n", lastEtage, lastExt);
  }

  float tMin = NAN, tMax = NAN;
  for (int i = 0; i < PANEL_W; i++) {
    if (!isnan(etageTemps[i])) {
      if (isnan(tMin) || etageTemps[i] < tMin) tMin = etageTemps[i];
      if (isnan(tMax) || etageTemps[i] > tMax) tMax = etageTemps[i];
    }
    if (!isnan(extTemps[i])) {
      if (isnan(tMin) || extTemps[i] < tMin) tMin = extTemps[i];
      if (isnan(tMax) || extTemps[i] > tMax) tMax = extTemps[i];
    }
  }
  Serial.printf("[dash] plage auto: tMin=%.1f tMax=%.1f\n", tMin, tMax);

  if (etageValid > 0 || extValid > 0)
    drawFills(etageTemps, extTemps, PANEL_W, tMin, tMax,
              Colors::blue(this->ledPanel->dma_display),
              Colors::red(this->ledPanel->dma_display));

  if (etageValid > 0) drawCurve(etageTemps, PANEL_W, tMin, tMax, Colors::blue(this->ledPanel->dma_display));
  if (extValid > 0)   drawCurve(extTemps,   PANEL_W, tMin, tMax, Colors::red(this->ledPanel->dma_display));

  drawCurrentValues(lastEtage, lastExt);
  drawVentilation(ventIsOn);

  if (etageValid == 0 && extValid == 0) {
    this->ledPanel->dma_display->setCursor(5, 30);
    this->ledPanel->dma_display->setTextColor(Colors::lightGrey(this->ledPanel->dma_display));
    this->ledPanel->dma_display->print("Pas de donnees");
  }

  Serial.println("[dash] == FIN BOUCLE ==");
}
