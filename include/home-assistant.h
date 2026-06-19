#ifndef HOME_ASSISTANT_H
#define HOME_ASSISTANT_H

#include <Arduino.h>

class HomeAssistant {
public:
  static String getEntityState(const String& entityId);
  // Remplit out[0..outSize-1] : un float par case, NAN si pas de donnee.
  // out[0] = il y a hoursBack heures, out[outSize-1] = maintenant.
  // Retourne le nombre de cases renseignees.
  static int getTimeSeries(const String& entityId, long hoursBack, float* out, int outSize);
};

#endif
