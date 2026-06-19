#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "led-panel.h"

#define GRAPH_LEFT 3
#define GRAPH_RIGHT 53
#define GRAPH_TOP 16
#define GRAPH_BOTTOM 53
#define GRAPH_WIDTH (GRAPH_RIGHT - GRAPH_LEFT)
#define GRAPH_HEIGHT (GRAPH_BOTTOM - GRAPH_TOP)
#define TEMP_MIN -10
#define TEMP_MAX 40
#define TEMP_RANGE (TEMP_MAX - TEMP_MIN)

class Dashboard {
  private:
    LEDPanel *ledPanel;
    void drawGrid();
    void drawCurve(const int16_t* temps, int count, uint16_t color, int minHour, int maxHour);
    void drawLegend();
    void drawYLabels();
    int tempToY(int16_t temp);
    int hourToX(int hour);
  public:
    Dashboard(LEDPanel *ledPanel);
    void setup();
    void loop();
};

#endif
