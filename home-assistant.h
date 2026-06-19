#ifndef HOME_ASSISTANT_H
#define HOME_ASSISTANT_H

#include <Arduino.h>

class HomeAssistant {
public:
  static String getEntityState(String entityName);
};

#endif