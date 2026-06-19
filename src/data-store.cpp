#include "data-store.h"

float DataStore::latestEtage = 0.0f;
float DataStore::latestExt = 0.0f;

void DataStore::store(float tempEtage, float tempExt) {
    latestEtage = tempEtage;
    latestExt = tempExt;
}

float DataStore::getLatestEtage() {
    return latestEtage;
}

float DataStore::getLatestExt() {
    return latestExt;
}
