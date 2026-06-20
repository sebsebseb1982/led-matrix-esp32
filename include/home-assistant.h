#ifndef HOME_ASSISTANT_H
#define HOME_ASSISTANT_H

#include <Arduino.h>
#include <time.h>

class HomeAssistant {
public:
  static String getEntityState(const String& entityId);
  // Remplit out[0..outSize-1] : un float par case, NAN si pas de donnee.
  // out[0] = il y a hoursBack heures, out[outSize-1] = maintenant.
  // Retourne le nombre de cases renseignees.
  static int getTimeSeries(const String& entityId, long hoursBack, float* out, int outSize);
  // Retourne les donnees brutes (timestamp Unix, valeur) des hoursBack dernieres heures.
  // Retourne le nombre de points stockes (max maxPoints).
  static int getRawSeries(const String& entityId, long hoursBack,
                          long* timestamps, float* values, int maxPoints);
  // Retourne les prochains lever/coucher du soleil depuis sun.sun.
  // Retourne false si l'appel echoue.
  static bool getSunTimes(time_t& nextRising, time_t& nextSetting);
};

#endif
