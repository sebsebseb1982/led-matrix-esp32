#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include <Arduino.h>

class WiFiConnection {
  public:
    static unsigned int nbConnection;
    static void setup();
    static void loop();
};

#endif