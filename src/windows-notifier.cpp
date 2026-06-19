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
    
    float tempEtage = DataStore::getLatestEtage();
    float tempExt = DataStore::getLatestExt();

    if (tempExt > tempEtage && this->upstairsShouldBeOpened) {
      this->upstairsShouldBeOpened = false;
      this->ledPanel->wakeUp();
      Buzzer::beepbeepbeep(400);
    }

    if (tempExt < tempEtage && !this->upstairsShouldBeOpened) {
      this->upstairsShouldBeOpened = true;
      this->ledPanel->wakeUp();
      Buzzer::beepbeepbeep(50);
    }
  }
}
