#ifndef HOME_ASSISTANT_H
#define HOME_ASSISTANT_H

#include <Arduino.h>

class HomeAssistant {
public:
  static String getEntityState(String entityName);
  // Remplit temps[0..numPixels-1] : un float par colonne pixel, NAN si pas de donnee.
  // pixel 0 = il y a hoursBack heures, pixel numPixels-1 = maintenant.
  // Retourne le nombre de pixels valides.
  static int getHistory(String entityName, float* temps, int numPixels, long hoursBack);
};

#endif
