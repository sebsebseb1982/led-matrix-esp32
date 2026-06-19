#include "data-store.h"
#include "home-assistant.h"

Data DataStore::get() {
  return {
    HomeAssistant::getEntityState("sensor.temperature_etage").toFloat(),
    HomeAssistant::getEntityState("sensor.temperature_rez_de_chaussee").toFloat(),
    HomeAssistant::getEntityState("sensor.domo_ext_rieur").toFloat()
  };
}