#include "dashboard.h"
#include "colors.h"
#include "weather-service.h"
#include <Arduino.h>
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

static uint16_t tempToColor(float temp, bool isInterior) {
  float coldStart, coldEnd, hotStart, hotEnd;
  if (isInterior) {
    coldStart = 16.0f; coldEnd = 19.0f;
    hotStart  = 24.0f; hotEnd  = 27.0f;
  } else {
    coldStart = 12.0f; coldEnd = 15.0f;
    hotStart  = 27.0f; hotEnd  = 30.0f;
  }
  uint8_t r, g, b;
  if (temp <= coldStart) {
    r = 0; g = 0; b = 255;
  } else if (temp < coldEnd) {
    float t = (temp - coldStart) / (coldEnd - coldStart);
    r = (uint8_t)(t * 255); g = (uint8_t)(t * 255); b = 255;
  } else if (temp <= hotStart) {
    r = 255; g = 255; b = 255;
  } else if (temp < hotEnd) {
    float t = (temp - hotStart) / (hotEnd - hotStart);
    r = 255; g = (uint8_t)((1.0f - t) * 255); b = (uint8_t)((1.0f - t) * 255);
  } else {
    r = 255; g = 0; b = 0;
  }
  return rgb565(r, g, b);
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

// Contour 8 directions puis le trait principal par-dessus, pour detacher texte
// et icones du fond des courbes. draw(dx, dy, couleur) fait le rendu decale.
static const int8_t OUTLINE_OFFSETS[8][2] = {{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}};

template <typename F>
static void drawWithOutline(F draw, uint16_t fg, uint16_t shadow) {
  for (auto& off : OUTLINE_OFFSETS) draw(off[0], off[1], shadow);
  draw(0, 0, fg);
}

void Dashboard::drawFills(const float* etageTemps, const float* extTemps,
                           int n, float tMin, float tMax) {
  int bottomY = SCREEN_HEIGHT - 1;

  bool isGridLine[SCREEN_HEIGHT] = {};
  if (!isnan(tMin) && !isnan(tMax) && tMax > tMin) {
    int firstTemp = (int)floorf(tMin / 10.0f) * 10;
    int lastTemp  = (int)ceilf(tMax / 10.0f) * 10;
    for (int temp = firstTemp; temp <= lastTemp; temp += 10) {
      float ratio = (temp - tMin) / (tMax - tMin);
      int y = bottomY - (int)(ratio * (bottomY - CURVE_PAD));
      if (y >= CURVE_PAD && y <= bottomY)
        isGridLine[y] = true;
    }
  }

  auto* disp = this->ledPanel->dma_display;

  // Précalcul : couleur par ligne Y selon la température que cette hauteur représente
  const int curveTopY    = CURVE_PAD;
  const int curveBottomY = SCREEN_HEIGHT - 1 - CURVE_PAD;
  uint16_t colorEtageAtY[SCREEN_HEIGHT] = {};
  uint16_t colorExtAtY[SCREEN_HEIGHT]   = {};
  if (tMax > tMin) {
    for (int y = CURVE_PAD; y <= bottomY; y++) {
      float ratio  = (float)(curveBottomY - y) / (float)(curveBottomY - curveTopY);
      float tempAtY = tMin + ratio * (tMax - tMin);
      colorEtageAtY[y] = tempToColor(tempAtY, true);
      colorExtAtY[y]   = tempToColor(tempAtY, false);
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

      int div = isGridLine[y] ? 3 : 7;
      uint16_t ce = dimColor565(colorEtageAtY[y], div);
      uint16_t cx = dimColor565(colorExtAtY[y],   div);

      if      (inEtage && inExt) disp->drawPixel(x, y, addColors565(ce, cx));
      else if (inEtage)          disp->drawPixel(x, y, ce);
      else                       disp->drawPixel(x, y, cx);
    }
  }
}

void Dashboard::drawCurve(const float* temps, int n, float tMin, float tMax, bool isInterior) {
  auto* disp = this->ledPanel->dma_display;
  for (int x = 1; x < n; x++) {
    if (isnan(temps[x]) || isnan(temps[x - 1])) continue;
    int y0 = tempToY(temps[x - 1], tMin, tMax);
    int y1 = tempToY(temps[x], tMin, tMax);
    float avgTemp = (temps[x - 1] + temps[x]) * 0.5f;
    disp->drawLine(x - 1, y0, x, y1, tempToColor(avgTemp, isInterior));
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

// Fleche droite avec queue : queue 6px + tete ">" 3px = 9px large, 5px haut (sans espace)
static void drawArrowRight(MatrixPanel_I2S_DMA* disp, int x, int y, uint16_t color) {
  // Queue horizontale au milieu (6px, accolee au chapeau)
  for (int i = 0; i < 6; i++) disp->drawPixel(x+i, y+2, color);
  // Tete ">"
  disp->drawPixel(x+6, y+0, color);
  disp->drawPixel(x+7, y+1, color);
  disp->drawPixel(x+8, y+2, color);
  disp->drawPixel(x+7, y+3, color);
  disp->drawPixel(x+6, y+4, color);
}

// Croix rouge 5x5
static void drawCross(MatrixPanel_I2S_DMA* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+0, y+0, color); disp->drawPixel(x+4, y+0, color);
  disp->drawPixel(x+1, y+1, color); disp->drawPixel(x+3, y+1, color);
  disp->drawPixel(x+2, y+2, color);
  disp->drawPixel(x+1, y+3, color); disp->drawPixel(x+3, y+3, color);
  disp->drawPixel(x+0, y+4, color); disp->drawPixel(x+4, y+4, color);
}

// Soleil 4x4 (jaune)
static void drawSun(MatrixPanel_I2S_DMA* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+1, y+0, color); disp->drawPixel(x+2, y+0, color);
  disp->drawPixel(x+0, y+1, color); disp->drawPixel(x+1, y+1, color); disp->drawPixel(x+2, y+1, color); disp->drawPixel(x+3, y+1, color);
  disp->drawPixel(x+0, y+2, color); disp->drawPixel(x+1, y+2, color); disp->drawPixel(x+2, y+2, color); disp->drawPixel(x+3, y+2, color);
  disp->drawPixel(x+1, y+3, color); disp->drawPixel(x+2, y+3, color);
}

// Lune croissant 4x4 (bleu clair) : C ouvert a droite
static void drawMoon(MatrixPanel_I2S_DMA* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+1, y+0, color); disp->drawPixel(x+2, y+0, color); disp->drawPixel(x+3, y+0, color);
  disp->drawPixel(x+0, y+1, color); disp->drawPixel(x+1, y+1, color);
  disp->drawPixel(x+0, y+2, color); disp->drawPixel(x+1, y+2, color);
  disp->drawPixel(x+1, y+3, color); disp->drawPixel(x+2, y+3, color); disp->drawPixel(x+3, y+3, color);
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
  bool isInterior;
  if (hasEtage && hasExt) {
    bool etageIsHigh = etageTemp >= extTemp;
    highTemp   = etageIsHigh ? etageTemp : extTemp;
    isInterior = etageIsHigh;
  } else if (hasEtage) {
    highTemp   = etageTemp;
    isInterior = true;
  } else {
    highTemp   = extTemp;
    isInterior = false;
  }
  uint16_t highColor = tempToColor(highTemp, isInterior);

  char buf[8];
  snprintf(buf, sizeof(buf), "%.1f", highTemp);
  for (int i = 0; buf[i]; i++) if (buf[i] == '.') { buf[i] = ','; break; }

  const int degCW    = 7;
  const int gap      = 0;
  const int rightPad = 1;
  int textW  = strlen(buf) * 6;
  int totalW = textW + gap + degCW + rightPad;
  int textX  = SCREEN_WIDTH - totalW;

  auto* disp = this->ledPanel->dma_display;
  disp->setTextSize(1);
  disp->setTextWrap(false);

  drawWithOutline([&](int dx, int dy, uint16_t c) {
    disp->setCursor(textX + dx, 1 + dy);
    disp->setTextColor(c);
    disp->print(buf);
    drawDegC(disp, textX + textW + gap + dx, 1 + dy, c);
  }, highColor, Colors::BLACK);
}

void Dashboard::drawVentilation(bool isOn) {
  auto* disp = this->ledPanel->dma_display;
  const int margin = 1;
  const int pad    = 1;
  const int symSz  = 5;
  int rx = margin;
  int ry = SCREEN_HEIGHT - margin - (2 * pad + symSz);
  disp->fillRect(rx, ry, 2 * pad + symSz, 2 * pad + symSz, Colors::VENT_BOX);
  if (isOn)
    drawCheckmark(disp, rx + pad, ry + pad, Colors::GREEN);
  else
    drawCross(disp, rx + pad, ry + pad, Colors::RED);

  float crossMins = WeatherService::crossingMinutes;
  if (isnan(crossMins)) return;

  char buf[8];
  int mins = (int)roundf(crossMins);
  if (mins < 60) {
    snprintf(buf, sizeof(buf), "%dm", mins);
  } else {
    int h = mins / 60;
    int m = mins % 60;
    if (m == 0)
      snprintf(buf, sizeof(buf), "%dh", h);
    else
      snprintf(buf, sizeof(buf), "%dh%02d", h, m);
  }

  int arrowX = rx + 2 * pad + symSz + 2;
  int textX  = arrowX + 9 + 1;  // fleche 9px large (6 queue + 3 tete) + 1px gap
  int arrowY = ry + 1;           // redescendu d'un cran
  int y      = ry;               // texte reste a ry (aligne avec la coche)

  drawWithOutline([&](int dx, int dy, uint16_t c) {
    drawArrowRight(disp, arrowX + dx, arrowY + dy, c);
  }, Colors::WHITE, Colors::BLACK);

  disp->setTextSize(1);
  disp->setTextWrap(false);
  drawWithOutline([&](int dx, int dy, uint16_t c) {
    disp->setCursor(textX + dx, y + dy);
    disp->setTextColor(c);
    disp->print(buf);
  }, Colors::WHITE, Colors::BLACK);
}

void Dashboard::drawSolarEvents(const float* extTemps, float tMin, float tMax) {
  auto* disp = this->ledPanel->dma_display;

  auto placeIcon = [&](int xPos, bool isSun) {
    if (xPos < 0 || xPos >= SCREEN_WIDTH) return;
    int curveY = (!isnan(tMin) && !isnan(tMax) && !isnan(extTemps[xPos]))
                 ? tempToY(extTemps[xPos], tMin, tMax)
                 : SCREEN_HEIGHT / 2;

    int iconX = max(0, min(SCREEN_WIDTH - 4, xPos - 2));
    int iconY = max(0, min(SCREEN_HEIGHT - 4, curveY - 2));

    if (isSun)
      drawSun(disp, iconX, iconY, Colors::SUN);
    else
      drawMoon(disp, iconX, iconY, Colors::MOON);
  };

  placeIcon(WeatherService::sunriseX, true);
  placeIcon(WeatherService::sunsetX,  false);
}

static void drawLoadingIcon(MatrixPanel_I2S_DMA* disp) {
  const int cx     = SCREEN_WIDTH  / 2;
  const int cy     = SCREEN_HEIGHT / 2;  // cadran centre sur l'ecran
  const int radius = 13;

  uint16_t cBody = Colors::LIGHT_GREY;
  uint16_t cTick = Colors::WHITE;
  uint16_t cHand = Colors::CLOCK_HAND;

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

void Dashboard::showLoading() {
  this->ledPanel->dma_display->clearScreen();
  this->ledPanel->dma_display->fillScreen(Colors::BLACK);
  drawLoadingIcon(this->ledPanel->dma_display);
}

void Dashboard::loop() {
  Serial.println("[dash] === BOUCLE ===");

  WeatherService::refresh();

  this->ledPanel->dma_display->clearScreen();
  this->ledPanel->dma_display->fillScreen(Colors::BLACK);

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
    drawFills(etageTemps, extTemps, SCREEN_WIDTH, tMin, tMax);

  if (etageValid > 0) drawCurve(etageTemps, SCREEN_WIDTH, tMin, tMax, true);
  if (extValid > 0)   drawCurve(extTemps,   SCREEN_WIDTH, tMin, tMax, false);

  if (extValid > 0) drawSolarEvents(extTemps, tMin, tMax);

  drawCurrentValues(WeatherService::lastEtage, WeatherService::lastExt);
  drawVentilation(WeatherService::ventIsOn);

  if (etageValid == 0 && extValid == 0) {
    this->ledPanel->dma_display->setCursor(5, 30);
    this->ledPanel->dma_display->setTextColor(Colors::LIGHT_GREY);
    this->ledPanel->dma_display->print("Pas de donnees");
  }

  Serial.println("[dash] == FIN BOUCLE ==");
}
