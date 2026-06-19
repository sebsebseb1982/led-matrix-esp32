#ifndef DATA_STORE_H
#define DATA_STORE_H

struct Data {
  float temperatureInsideUpstairs;
  float temperatureInsideDownstairs;
  float temperatureOutside;
};

class DataStore {
  public:
    static Data get();
};

#endif