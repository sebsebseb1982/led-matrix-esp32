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
  // Requetes par tranches de chunkHours : plus court = moins de RAM, plus de requetes.
  static int getTimeSeries(const String& entityId, long hoursBack, float* out, int outSize,
                           long chunkHours = 4);
  // Marque out[x] = true pour chaque colonne ou le thermostat chauffait
  // (attribut hvac_action == "heating"). Meme axe que getTimeSeries.
  // Retourne le nombre de colonnes en chauffe.
  static int getHeatingColumns(const String& entityId, long hoursBack, bool* out, int outSize,
                               long chunkHours = 4);
  // Etat courant du thermostat : mode chauffe (etat "heat") et consigne.
  // Retourne false si l'appel echoue.
  static bool getClimateState(const String& entityId, bool& heatMode, float& setpoint);
  // Retourne les donnees brutes (timestamp Unix, valeur) des hoursBack dernieres heures.
  // Retourne le nombre de points stockes (max maxPoints).
  static int getRawSeries(const String& entityId, long hoursBack,
                          long* timestamps, float* values, int maxPoints);
  // Retourne les prochains lever/coucher du soleil depuis sun.sun.
  // Retourne false si l'appel echoue.
  static bool getSunTimes(time_t& nextRising, time_t& nextSetting);
  // Prevision horaire (service weather.get_forecasts), points chronologiques.
  // Retourne le nombre de points stockes (max maxPoints), 0 si l'appel echoue.
  static int getHourlyForecast(const String& entityId, long* timestamps, float* temps, int maxPoints);
};

#endif
