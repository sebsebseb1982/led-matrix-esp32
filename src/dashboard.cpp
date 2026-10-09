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

struct RGBf { float r, g, b; };

static RGBf tempToRGB(float temp, bool isInterior) {
  float coldStart, coldEnd, hotStart, hotEnd;
  if (isInterior) {
    coldStart = 16.0f; coldEnd = 19.0f;
    hotStart  = 24.0f; hotEnd  = 27.0f;
  } else {
    coldStart = 12.0f; coldEnd = 15.0f;
    hotStart  = 27.0f; hotEnd  = 30.0f;
  }
  if (temp <= coldStart) {
    return {0, 0, 255};
  } else if (temp < coldEnd) {
    float t = (temp - coldStart) / (coldEnd - coldStart);
    return {t * 255, t * 255, 255};
  } else if (temp <= hotStart) {
    return {255, 255, 255};
  } else if (temp < hotEnd) {
    float t = (temp - hotStart) / (hotEnd - hotStart);
    return {255, (1.0f - t) * 255, (1.0f - t) * 255};
  } else {
    return {255, 0, 0};
  }
}

static uint16_t tempToColor(float temp, bool isInterior) {
  RGBf c = tempToRGB(temp, isInterior);
  return rgb565((uint8_t)c.r, (uint8_t)c.g, (uint8_t)c.b);
}

// Le remplissage est tres attenue (1/7) : en RGB565 il ne reste alors que
// 5 niveaux en rouge/bleu, d'ou des bandes. Tramage ordonne Bayer 4x4 : chaque
// pixel arrondit vers le haut ou le bas selon sa position, ce qui restitue les
// niveaux intermediaires a l'oeil.
static const uint8_t BAYER4[4][4] = {
  { 0,  8,  2, 10},
  {12,  4, 14,  6},
  { 3, 11,  1,  9},
  {15,  7, 13,  5},
};

static uint16_t ditherTo565(RGBf c, int x, int y) {
  float threshold = (BAYER4[y & 3][x & 3] + 0.5f) / 16.0f;
  auto q = [&](float v, int maxLevel) {
    float lv = v * maxLevel / 255.0f;
    int   i  = (int)lv;
    if (lv - i > threshold) i++;
    return min(i, maxLevel);
  };
  // Vert quantifie sur 5 bits comme rouge et bleu : sinon, sur les teintes ou
  // r == g, les deux canaux n'arrondissent pas pareil et des points roses
  // apparaissent.
  return (uint16_t)((q(c.r, 31) << 11) | ((q(c.g, 31) << 1) << 5) | q(c.b, 31));
}

static uint16_t dimColor565(uint16_t c, int div) {
  uint8_t r = ((c >> 11) & 0x1F) / div;
  uint8_t g = ((c >> 5)  & 0x3F) / div;
  uint8_t b = (c          & 0x1F) / div;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// Contour 8 directions puis le trait principal par-dessus, pour detacher texte
// et icones du fond des courbes. Le contour assombrit le fond au lieu de le
// noircir : les decalages sont d'abord rendus dans un masque, pour que chaque
// pixel ne soit assombri qu'une fois malgre les recouvrements.
// draw(cible, dx, dy, couleur) fait le rendu decale.
static const int8_t OUTLINE_OFFSETS[8][2] = {{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}};
#define OUTLINE_DIM 4

template <typename F>
static void drawWithOutline(GFXcanvas16* canvas, F draw, uint16_t fg) {
  static GFXcanvas1 mask(SCREEN_WIDTH, SCREEN_HEIGHT);
  mask.fillScreen(0);
  for (auto& off : OUTLINE_OFFSETS) draw(&mask, off[0], off[1], 1);
  for (int y = 0; y < SCREEN_HEIGHT; y++)
    for (int x = 0; x < SCREEN_WIDTH; x++)
      if (mask.getPixel(x, y))
        canvas->drawPixel(x, y, dimColor565(canvas->getPixel(x, y), OUTLINE_DIM));
  draw(canvas, 0, 0, fg);
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

  GFXcanvas16* disp = this->frame;

  // Précalcul : couleur par ligne Y selon la température que cette hauteur représente
  const int curveTopY    = CURVE_PAD;
  const int curveBottomY = SCREEN_HEIGHT - 1 - CURVE_PAD;
  // (en flottant : l'attenuation se fait avant la quantification 565)
  RGBf colorEtageAtY[SCREEN_HEIGHT] = {};
  RGBf colorExtAtY[SCREEN_HEIGHT]   = {};
  if (tMax > tMin) {
    for (int y = CURVE_PAD; y <= bottomY; y++) {
      float ratio  = (float)(curveBottomY - y) / (float)(curveBottomY - curveTopY);
      float tempAtY = tMin + ratio * (tMax - tMin);
      colorEtageAtY[y] = tempToRGB(tempAtY, true);
      colorExtAtY[y]   = tempToRGB(tempAtY, false);
    }
  }

  for (int x = 0; x < n; x++) {
    bool hasEtage = !isnan(etageTemps[x]);
    bool hasExt   = !isnan(extTemps[x]);
    int yEtage = hasEtage ? tempToY(etageTemps[x], tMin, tMax) : bottomY + 1;
    int yExt   = hasExt   ? tempToY(extTemps[x],   tMin, tMax) : bottomY + 1;
    bool heating = WeatherService::heatingOn[x];

    for (int y = CURVE_PAD; y <= bottomY; y++) {
      bool inEtage = hasEtage && y > yEtage;
      bool inExt   = hasExt   && y > yExt;
      if (!inEtage && !inExt) continue;

      // Ligne des dizaines en tirets : 5 pixels allumes, 2 eteints
      bool gridDot = isGridLine[y] && (x % 7 < 5);
      float div = gridDot ? 3.0f : 7.0f;
      RGBf c = {0, 0, 0};
      if (inEtage) {
        c.r += colorEtageAtY[y].r / div;
        c.g += colorEtageAtY[y].g / div;
        c.b += colorEtageAtY[y].b / div;
        if (heating) c.r += Colors::HEAT_TINT_R;
      }
      if (inExt) {
        c.r += colorExtAtY[y].r / div;
        c.g += colorExtAtY[y].g / div;
        c.b += colorExtAtY[y].b / div;
      }
      c.r = min(c.r, 255.0f); c.g = min(c.g, 255.0f); c.b = min(c.b, 255.0f);
      disp->drawPixel(x, y, ditherTo565(c, x, y));
    }
  }
}

void Dashboard::drawCurve(const float* temps, int n, float tMin, float tMax, bool isInterior) {
  GFXcanvas16* disp = this->frame;
  for (int x = 1; x < n; x++) {
    if (isnan(temps[x]) || isnan(temps[x - 1])) continue;
    int y0 = tempToY(temps[x - 1], tMin, tMax);
    int y1 = tempToY(temps[x], tMin, tMax);
    // Couleur par pixel (temperature interpolee le long du segment) : une
    // couleur unique par segment ecrase le degrade sur les fortes pentes.
    int dy    = y1 - y0;
    int steps = max(1, abs(dy));
    for (int i = 0; i <= steps; i++) {
      float f  = (float)i / steps;
      int px   = (f < 0.5f) ? x - 1 : x;
      int py   = y0 + dy * i / steps;
      float t  = temps[x - 1] + f * (temps[x] - temps[x - 1]);
      disp->drawPixel(px, py, tempToColor(t, isInterior));
    }
  }
}

// Coche verte 5x4 : bras droit (4,0)->(2,2), pointe (1,3), bras gauche (0,2)->(1,3)
static void drawCheckmark(Adafruit_GFX* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+4, y+0, color);
  disp->drawPixel(x+3, y+1, color);
  disp->drawPixel(x+2, y+2, color);
  disp->drawPixel(x+0, y+2, color);
  disp->drawPixel(x+1, y+3, color);
}

// Fleche droite avec queue : queue 6px + tete ">" 3px = 9px large, 5px haut (sans espace)
static void drawArrowRight(Adafruit_GFX* disp, int x, int y, uint16_t color) {
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
static void drawCross(Adafruit_GFX* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+0, y+0, color); disp->drawPixel(x+4, y+0, color);
  disp->drawPixel(x+1, y+1, color); disp->drawPixel(x+3, y+1, color);
  disp->drawPixel(x+2, y+2, color);
  disp->drawPixel(x+1, y+3, color); disp->drawPixel(x+3, y+3, color);
  disp->drawPixel(x+0, y+4, color); disp->drawPixel(x+4, y+4, color);
}

// Soleil 4x4 (jaune)
static void drawSun(Adafruit_GFX* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+1, y+0, color); disp->drawPixel(x+2, y+0, color);
  disp->drawPixel(x+0, y+1, color); disp->drawPixel(x+1, y+1, color); disp->drawPixel(x+2, y+1, color); disp->drawPixel(x+3, y+1, color);
  disp->drawPixel(x+0, y+2, color); disp->drawPixel(x+1, y+2, color); disp->drawPixel(x+2, y+2, color); disp->drawPixel(x+3, y+2, color);
  disp->drawPixel(x+1, y+3, color); disp->drawPixel(x+2, y+3, color);
}

// Lune croissant 4x4 (bleu clair) : C ouvert a droite
static void drawMoon(Adafruit_GFX* disp, int x, int y, uint16_t color) {
  disp->drawPixel(x+1, y+0, color); disp->drawPixel(x+2, y+0, color); disp->drawPixel(x+3, y+0, color);
  disp->drawPixel(x+0, y+1, color); disp->drawPixel(x+1, y+1, color);
  disp->drawPixel(x+0, y+2, color); disp->drawPixel(x+1, y+2, color);
  disp->drawPixel(x+1, y+3, color); disp->drawPixel(x+2, y+3, color); disp->drawPixel(x+3, y+3, color);
}

// degC en pixel art : deg (3x3) + gap (1px) + C (3x5) = 7px wide, 5px tall
static void drawDegC(Adafruit_GFX* disp, int x, int y, uint16_t color) {
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

  GFXcanvas16* disp = this->frame;
  drawWithOutline(disp, [&](Adafruit_GFX* g, int dx, int dy, uint16_t c) {
    g->setTextSize(1);
    g->setTextWrap(false);
    g->setCursor(textX + dx, 1 + dy);
    g->setTextColor(c);
    g->print(buf);
    drawDegC(g, textX + textW + gap + dx, 1 + dy, c);
  }, highColor);
}

void Dashboard::drawVentilation(bool isOn) {
  GFXcanvas16* disp = this->frame;
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

  drawWithOutline(disp, [&](Adafruit_GFX* g, int dx, int dy, uint16_t c) {
    drawArrowRight(g, arrowX + dx, arrowY + dy, c);
  }, Colors::WHITE);

  drawWithOutline(disp, [&](Adafruit_GFX* g, int dx, int dy, uint16_t c) {
    g->setTextSize(1);
    g->setTextWrap(false);
    g->setCursor(textX + dx, y + dy);
    g->setTextColor(c);
    g->print(buf);
  }, Colors::WHITE);
}

void Dashboard::drawSolarEvents(const float* extTemps, float tMin, float tMax) {
  GFXcanvas16* disp = this->frame;

  auto placeIcon = [&](int xPos, bool isSun) {
    if (xPos < 0 || xPos >= SCREEN_WIDTH) return;
    int curveY = (!isnan(tMin) && !isnan(tMax) && !isnan(extTemps[xPos]))
                 ? tempToY(extTemps[xPos], tMin, tMax)
                 : SCREEN_HEIGHT / 2;

    // Icone 4x4 + contour d'1px : on la garde a 1px des bords pour que le contour reste visible
    int iconX = max(1, min(SCREEN_WIDTH - 5, xPos - 2));
    int iconY = max(1, min(SCREEN_HEIGHT - 5, curveY - 2));

    drawWithOutline(disp, [&](Adafruit_GFX* g, int dx, int dy, uint16_t c) {
      if (isSun)
        drawSun(g, iconX + dx, iconY + dy, c);
      else
        drawMoon(g, iconX + dx, iconY + dy, c);
    }, isSun ? Colors::SUN : Colors::MOON);
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
  this->frame    = new GFXcanvas16(SCREEN_WIDTH, SCREEN_HEIGHT);
}

void Dashboard::showLoading() {
  this->ledPanel->dma_display->clearScreen();
  this->ledPanel->dma_display->fillScreen(Colors::BLACK);
  drawLoadingIcon(this->ledPanel->dma_display);
}

void Dashboard::loop() {
  Serial.println("[dash] === BOUCLE ===");

  WeatherService::refresh();

  this->frame->fillScreen(Colors::BLACK);

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
    this->frame->setCursor(5, 30);
    this->frame->setTextColor(Colors::LIGHT_GREY);
    this->frame->print("Pas de donnees");
  }

  this->ledPanel->dma_display->drawRGBBitmap(0, 0, this->frame->getBuffer(), SCREEN_WIDTH, SCREEN_HEIGHT);

  Serial.println("[dash] == FIN BOUCLE ==");
}
