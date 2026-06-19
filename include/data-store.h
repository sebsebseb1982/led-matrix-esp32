#ifndef DATA_STORE_H
#define DATA_STORE_H

#include <stdint.h>

#define MAX_READINGS 288

typedef struct {
  uint16_t seq;
  uint8_t heure;    // heure du relevé (0-23)
  int16_t tempEtage; // étage temperature * 10 (e.g., 215 = 21.5°C)
  int16_t tempExt;   // extérieur temperature * 10
} Reading;

class DataStore {
  private:
    static const char* NVS_NAMESPACE;
    static const char* NVS_KEY_COUNT;
    static const char* NVS_KEY_DATA;
  public:
    static void setup();
    static void store(float tempEtage, float tempExt, uint8_t heure);
    static int getCount();
    static Reading getReading(int index);
    static float getLatestEtage();
    static float getLatestExt();
};

#endif
