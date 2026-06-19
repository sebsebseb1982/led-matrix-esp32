#ifndef HOME_ASSISTANT_H
#define HOME_ASSISTANT_H

#include <Arduino.h>

typedef struct {
  int ts;
  float value;
} HistoryPoint;

class HomeAssistant {
public:
  static String getEntityState(String entityName);
  static int getHistory(String entityName, HistoryPoint* points, int maxPoints, long hoursBack);
};

#endif