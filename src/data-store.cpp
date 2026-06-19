#include "data-store.h"
#include <Preferences.h>

const char* DataStore::NVS_NAMESPACE = "tempstore";
const char* DataStore::NVS_KEY_COUNT = "count";
const char* DataStore::NVS_KEY_DATA = "data";

void DataStore::setup() {
}

void DataStore::store(float tempEtage, float tempExt) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    return;
  }
  
  uint16_t count = prefs.getUShort(NVS_KEY_COUNT, 0);
  
  Reading newReading;
  newReading.seq = count;
  newReading.tempEtage = (int16_t)(tempEtage * 10);
  newReading.tempExt = (int16_t)(tempExt * 10);
  
  Reading readings[MAX_READINGS];
  
  if (count > 0) {
    prefs.getBytes(NVS_KEY_DATA, readings, count * sizeof(Reading));
  }
  
  readings[count++] = newReading;
  
  if (count > MAX_READINGS) {
    count = MAX_READINGS;
    memmove(readings, readings + (count - MAX_READINGS), MAX_READINGS * sizeof(Reading));
  }
  
  prefs.putUShort(NVS_KEY_COUNT, count);
  prefs.putBytes(NVS_KEY_DATA, readings, MAX_READINGS * sizeof(Reading));
  
  prefs.end();
}

int DataStore::getCount() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {
    return 0;
  }
  int count = prefs.getUShort(NVS_KEY_COUNT, 0);
  prefs.end();
  return count;
}

Reading DataStore::getReading(int index) {
  Reading empty = {0, 0, 0};
  
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {
    prefs.end();
    return empty;
  }
  
  uint16_t count = prefs.getUShort(NVS_KEY_COUNT, 0);
  if (index < 0 || index >= count) {
    prefs.end();
    return empty;
  }
  
  Reading readings[MAX_READINGS];
  prefs.getBytes(NVS_KEY_DATA, readings, count * sizeof(Reading));
  prefs.end();
  
  return readings[index];
}

float DataStore::getLatestEtage() {
  int count = getCount();
  if (count == 0) return 0.0;
  Reading r = getReading(count - 1);
  return r.tempEtage / 10.0;
}

float DataStore::getLatestExt() {
  int count = getCount();
  if (count == 0) return 0.0;
  Reading r = getReading(count - 1);
  return r.tempExt / 10.0;
}

int DataStore::getHoursAgo(int index) {
  int count = getCount();
  if (count == 0) return 24;
  
  int readingsPerHour = 12;
  int hoursAgo = (count - 1 - index) / readingsPerHour;
  if (hoursAgo < 0) hoursAgo = 0;
  if (hoursAgo > 24) hoursAgo = 24;
  return hoursAgo;
}
