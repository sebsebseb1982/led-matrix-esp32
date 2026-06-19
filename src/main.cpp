#include "wifi-connection.h"
#include "ota.h"
#include "led-panel.h"
#include "colors.h"
#include "clock.h"
#include "ripples.h"
#include "dashboard.h"
#include "bitmap.h"
#include "brightness.h"
#include "buzzer.h"
#include "pir-sensor.h"
#include "windows-notifier.h"

LEDPanel ledPanel(8);
Clock myClock(&ledPanel);
Ripples ripples(&ledPanel);
Dashboard dashboard(&ledPanel);
Bitmap bitmap(&ledPanel);
Brightness brightness(&ledPanel);
//WindowsNotifier windowsNotifier(&ledPanel);

void setup() {
  Serial.begin(115200);
  Serial.println("setup()");
  WiFiConnection::setup();
  OTA::setup();
  ledPanel.setup();
  //ripples.setup();
  //myClock.setup();
  //bitmap.setup();
  dashboard.setup();
  brightness.setup();
  Buzzer::setup();
  PIRSensor::setup();
  //windowsNotifier.setup();

  // Display
  brightness.loop();
  dashboard.loop();

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, HIGH);

  //Rentre en mode Deep Sleep
  Serial.println("Rentre en mode Deep Sleep");
  Serial.println("----------------------");
  esp_deep_sleep_start();
}

void loop() {
  /*Serial.println("loop()");
  WiFiConnection::loop();
  OTA::loop();
  Buzzer::loop();
  ledPanel.loop();
  windowsNotifier.loop();
  if (!ledPanel.isStandby()) {
    brightness.loop();
    ripples.loop();
    myClock.loop();
    bitmap.loop();
    dashboard.loop();
  }*/
}