#ifndef DATA_STORE_H
#define DATA_STORE_H

#include <stdint.h>

class DataStore {
private:
    static float latestEtage;
    static float latestExt;
public:
    static void store(float tempEtage, float tempExt);
    static float getLatestEtage();
    static float getLatestExt();
};

#endif
