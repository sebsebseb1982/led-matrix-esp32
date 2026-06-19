#ifndef WINDOWS_NOTIFIER_H
#define WINDOWS_NOTIFIER_H

#include "led-panel.h"

#define checkEveryNSeconds 180

class WindowsNotifier {
private:
  bool upstairsShouldBeOpened;
  bool downstairsShouldBeOpened;
  unsigned long nextCheckInMs;
  LEDPanel *ledPanel;
public:
  WindowsNotifier(LEDPanel *ledPanel);
  void setup();
  void loop();
};

#endif