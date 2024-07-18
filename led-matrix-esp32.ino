#include "wifi-connection.h"
#include "ota.h"
#include "led-panel.h"
#include "colors.h"
#include "clock.h"
#include "ripples.h"
#include "bitmap.h"
#include "brightness.h"

LEDPanel ledPanel(8);
Clock myClock(&ledPanel);
Ripples ripples(&ledPanel);
Bitmap bitmap(&ledPanel);
Brightness brightness(&ledPanel);

void setup() {
  Serial.begin(115200);
  Serial.println("setup()");
  WiFiConnection::setup();
  OTA::setup();
  ledPanel.setup();
  //ripples.setup();
  //myClock.setup();
  bitmap.setup();
  brightness.setup();
}

void loop() {
  Serial.println("loop()");
  WiFiConnection::loop();
  OTA::loop();
  ledPanel.loop();
  brightness.loop();
  //ripples.loop();
  //myClock.loop();
  bitmap.loop();
}