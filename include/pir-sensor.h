#ifndef PIR_SENSOR_H
#define PIR_SENSOR_H

class PIRSensor {
  public:
    static void setup();
    static bool isTriggered();
};

#endif