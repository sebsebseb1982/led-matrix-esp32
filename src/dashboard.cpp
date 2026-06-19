#include "dashboard.h"
#include "colors.h"
#include "weather-service.h"
#include <Arduino.h>
#include <time.h>
#include <math.h>

#define CURVE_PAD 4

int Dashboard::tempToY(float temp, float tMin, float tMax) {
  float range = tMax - tMin;
  float ratio = (range == 0.0f) ? 0.5f : (temp - tMin) / range;
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;
  int topY    = CURVE_PAD;
  int bottomY = SCREEN_HEIGHT - 1 - CURVE_PAD;
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
                           int n, float tMin, float tMax,
                           uint16_t etageColor, uint16_t extColor) {
  // 3 niveaux : fill de base /7, ligne d'unité /4, ligne de dizaine /2
  uint16_t dimEtage    = dimColor565(etageColor, 7);
  uint16_t dimExt      = dimColor565(extColor,   7);
  uint16_t decadeEtage = dimColor565(etageColor, 2);
  uint16_t decadeExt   = dimColor565(extColor,   2);
  int bottomY = SCREEN_HEIGHT - 1;

  // Tableau indexé par Y : 0=fill, 1=unité, 2=dizaine.
  // Y calculé sur bottomY=63 (sans padding bas) pour autoriser les lignes dans les 4px du bas.
  int8_t gridType[SCREEN_HEIGHT] = {};
  if (!isnan(tMin) && !isnan(tMax) && tMax > tMin) {
    int firstTemp = (int)floorf(tMin / 10.0f) * 10;
    int lastTemp  = (int)ceilf(tMax / 10.0f) * 10;
    for (int temp = firstTemp; temp <= lastTemp; temp += 10) {
      float ratio = (temp - tMin) / (tMax - tMin);
      int y = bottomY - (int)(ratio * (bottomY - CURVE_PAD));
      if (y >= CURVE_PAD && y <= bottomY)
        gridType[y] = 2;
    }
  }

  for (int x = 0; x < n; x++) {
    bool hasEtage = !isnan(etageTemps[x]);
    bool hasExt   = !isnan(extTemps[x]);
    int yEtage = hasEtage ? tempToY(etageTemps[x], tMin, tMax) : bottomY + 1;
    int yExt   = hasExt   ? tempToY(extTemps[x],   tMin, tMax) : bottomY + 1;

    for (int y = CURVE_PAD; y <= bottomY; y++) {
      bool inEtage = hasEtage && y > yEtage;
      bool inExt   = hasExt   && y > yExt;
      if (!inEtage && !inExt) continue;

      uint16_t ce = (gridType[y] == 2) ? decadeEtage : dimEtage;
      uint16_t cx = (gridType[y] == 2) ? decadeExt   : dimExt;

      if      (inEtage && inExt) this->ledPanel->dma_display->drawPixel(x, y, addColors565(ce, cx));
      else if (inEtage)           this->ledPanel->dma_display->drawPixel(x, y, ce);
      else if (inExt)             this->ledPanel->dma_display->drawPixel(x, y, cx);
    }
  }
}

void Dashboard::drawCurve(const float* temps, int n, float tMin, float tMax, uint16_t color) {
  for (int x = 1; x < n; x++) {
    if (isnan(temps[x]) || isnan(temps[x - 1])) continue;
    int y0 = tempToY(temps[x - 1], tMin, tMax);
    int y1 = tempToY(temps[x], tMin, tMax);
    this->ledPanel->dma_display->drawLine(x - 1, y0, x, y1, color);
  }
}

// Coche verte 5x4 : bras droit (4,0)->(2,2), pointe (1,3), bras gauche (0,2)->(1,3)
static void drawCheckmark(MatrixPanel_I2S_DMA* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+4, y+0, color);
  disp->drawPixel(x+3, y+1, color);
  disp->drawPixel(x+2, y+2, color);
  disp->drawPixel(x+0, y+2, color);
  disp->drawPixel(x+1, y+3, color);
}

// Croix rouge 5x5
static void drawCross(MatrixPanel_I2S_DMA* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+0, y+0, color); disp->drawPixel(x+4, y+0, color);
  disp->drawPixel(x+1, y+1, color); disp->drawPixel(x+3, y+1, color);
  disp->drawPixel(x+2, y+2, color);
  disp->drawPixel(x+1, y+3, color); disp->drawPixel(x+3, y+3, color);
  disp->drawPixel(x+0, y+4, color); disp->drawPixel(x+4, y+4, color);
}

// degC en pixel art : deg (3x3) + gap (1px) + C (3x5) = 7px wide, 5px tall
static void drawDegC(MatrixPanel_I2S_DMA* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+1, y+0, color);
  disp->drawPixel(x+0, y+1, color);
  disp->drawPixel(x+2, y+1, color);
  disp->drawPixel(x+1, y+2, color);
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

  const int degCW    = 7;
  const int gap      = 0;
  const int rightPad = 1;
  int textW  = strlen(buf) * 6;
  int totalW = textW + gap + degCW + rightPad;
  int textX  = SCREEN_WIDTH - totalW;

  auto* disp = this->ledPanel->dma_display;
  uint16_t shadowColor = Colors::black(disp);
  disp->setTextSize(1);
  disp->setTextWrap(false);

  const int8_t offsets[8][2] = {{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}};
  for (auto& off : offsets) {
    disp->setCursor(textX + off[0], 1 + off[1]);
    disp->setTextColor(shadowColor);
    disp->print(buf);
    drawDegC(disp, textX + textW + gap + off[0], 1 + off[1], shadowColor);
  }

  disp->setCursor(textX, 1);
  disp->setTextColor(highColor);
  disp->print(buf);
  drawDegC(disp, textX + textW + gap, 1, highColor);
}

void Dashboard::drawVentilation(bool isOn) {
  auto* disp = this->ledPanel->dma_display;
  const int margin = 1;
  const int pad    = 1;
  const int symSz  = 5;
  int rx = margin;
  int ry = SCREEN_HEIGHT - margin - (2 * pad + symSz);
  disp->fillRect(rx, ry, 2 * pad + symSz, 2 * pad + symSz, Colors::rgb(disp, 150, 150, 150));
  if (isOn)
    drawCheckmark(disp, rx + pad, ry + pad, Colors::green(disp));
  else
    drawCross(disp, rx + pad, ry + pad, Colors::red(disp));
}

static void drawLoadingIcon(MatrixPanel_I2S_DMA* disp) {
  const int cx     = SCREEN_WIDTH  / 2;
  const int cy     = SCREEN_HEIGHT / 2;  // cadran centre sur l'ecran
  const int radius = 13;

  uint16_t cBody = Colors::lightGrey(disp);
  uint16_t cTick = Colors::white(disp);
  uint16_t cHand = Colors::rgb(disp, 255, 180, 0);  // ambre

  // Couronne (base uniquement, se connecte au cercle)
  disp->fillRect(cx - 4, cy - radius - 3, 9, 3, cBody);

  // Boitier
  disp->drawCircle(cx, cy, radius, cBody);

  // Reperes horaires (3px vers l'interieur depuis le bord)
  disp->drawLine(cx + radius - 2, cy,          cx + radius, cy,          cTick);  // 3h
  disp->drawLine(cx,              cy + radius - 2, cx,       cy + radius, cTick);  // 6h
  disp->drawLine(cx - radius,     cy,          cx - radius + 2, cy,       cTick);  // 9h

  // Aiguille vers 12h
  disp->drawLine(cx, cy, cx, cy - radius + 3, cHand);

  // Moyeu central
  disp->fillRect(cx - 1, cy - 1, 3, 3, cTick);
}

Dashboard::Dashboard(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Dashboard::setup() {
  Serial.println("[dash] setup OK");
}

void Dashboard::showLoading() {
  this->ledPanel->dma_display->clearScreen();
  this->ledPanel->dma_display->fillScreen(Colors::black(this->ledPanel->dma_display));
  drawLoadingIcon(this->ledPanel->dma_display);
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

  WeatherService::refresh();

  this->ledPanel->dma_display->clearScreen();
  this->ledPanel->dma_display->fillScreen(Colors::black(this->ledPanel->dma_display));

  const float* etageTemps = WeatherService::etageTemps;
  const float* extTemps   = WeatherService::extTemps;
  int etageValid = WeatherService::etageValid;
  int extValid   = WeatherService::extValid;

  float tMin = NAN, tMax = NAN;
  for (int i = 0; i < SCREEN_WIDTH; i++) {
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
    drawFills(etageTemps, extTemps, SCREEN_WIDTH, tMin, tMax,
              Colors::blue(this->ledPanel->dma_display),
              Colors::red(this->ledPanel->dma_display));

  if (etageValid > 0) drawCurve(etageTemps, SCREEN_WIDTH, tMin, tMax, Colors::blue(this->ledPanel->dma_display));
  if (extValid > 0)   drawCurve(extTemps,   SCREEN_WIDTH, tMin, tMax, Colors::red(this->ledPanel->dma_display));

  drawCurrentValues(WeatherService::lastEtage, WeatherService::lastExt);
  drawVentilation(WeatherService::ventIsOn);

  if (etageValid == 0 && extValid == 0) {
    this->ledPanel->dma_display->setCursor(5, 30);
    this->ledPanel->dma_display->setTextColor(Colors::lightGrey(this->ledPanel->dma_display));
    this->ledPanel->dma_display->print("Pas de donnees");
  }

  Serial.println("[dash] == FIN BOUCLE ==");
}
