#include "windows-notifier.h"
#include "data-store.h"
#include "buzzer.h"

WindowsNotifier::WindowsNotifier(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
  this->nextCheckInMs = millis();
}

void WindowsNotifier::setup() {
  this->loop();
}

void WindowsNotifier::loop() {
  if (millis() > this->nextCheckInMs) {
    this->nextCheckInMs = millis() + (checkEveryNSeconds * 1000);
    Data data = DataStore::get();

    if (data.temperatureOutside > data.temperatureInsideUpstairs && this->upstairsShouldBeOpened) {
      this->upstairsShouldBeOpened = false;
      this->ledPanel->wakeUp();
      Buzzer::beepbeepbeep(400);
    }

    if (data.temperatureOutside > data.temperatureInsideDownstairs && this->downstairsShouldBeOpened) {
      this->downstairsShouldBeOpened = false;
      this->ledPanel->wakeUp();
      Buzzer::beepbeepbeep(400);
    }

    if (data.temperatureOutside < data.temperatureInsideUpstairs && !this->upstairsShouldBeOpened) {
      this->upstairsShouldBeOpened = true;
      this->ledPanel->wakeUp();
      Buzzer::beepbeepbeep(50);
    }

    if (data.temperatureOutside < data.temperatureInsideUpstairs && !this->upstairsShouldBeOpened) {
      this->upstairsShouldBeOpened = true;
      this->ledPanel->wakeUp();
      Buzzer::beepbeepbeep(50);
    }
  }
}