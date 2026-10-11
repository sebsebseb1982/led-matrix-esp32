#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "led-panel.h"

// 16 colonnes de prevision x 2 courbes, avec de la marge pour les fortes pentes.
#define SCAN_MAX_PX 512

class Dashboard {
  private:
    struct ScanPixel { uint8_t x, y; };

    LEDPanel *ledPanel;
    // Framebuffers en RAM : la dalle ne se relit pas, or le contour assombrit
    // les pixels deja dessines. Deux images, prevision attenuee et vive, entre
    // lesquelles animate() melange pour le balayage. frame = cible du rendu en cours.
    GFXcanvas16 *frameDim;
    GFXcanvas16 *frameLit;
    GFXcanvas16 *frame;
    bool forecastBright = false;
    ScanPixel scan[SCAN_MAX_PX];
    int scanCount = 0;
    unsigned long lastScanMs = 0;

    int tempToY(float temp, float tMin, float tMax);
    void render();
    void drawFills(const float* etageTemps, const float* extTemps, int n, float tMin, float tMax);
    void drawCurve(const float* temps, int n, float tMin, float tMax, bool isInterior);
    void drawCurrentValues(float etageTemp, float extTemp);
    void drawVentilation(bool isOn);
    void drawSolarEvents(const float* extTemps, float tMin, float tMax);
    void drawHourTicks();
  public:
    Dashboard(LEDPanel *ledPanel);
    void showLoading();
    void update();
    void animate();
};

#endif
