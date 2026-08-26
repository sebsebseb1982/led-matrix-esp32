#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include <Arduino.h>

class WiFiConnection {
  public:
    static void setup();
    // Reconnecte si le lien est tombe. A appeler avant chaque cycle de fetch.
    static void loop();
};

#endif
