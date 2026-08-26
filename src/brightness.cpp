#include "brightness.h"

Brightness::Brightness(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Brightness::loop() {
  int raw = analogRead(LIGHT_SENSOR_PIN);

  // ponytail: lissage exponentiel, l'ADC de l'ESP32 est bruite et la luminosite
  // scintillait d'un cycle a l'autre. Moyenne glissante si ca ne suffit pas.
  // Premier appel : on part de la mesure, sinon la rampe depuis 0 laisse
  // l'ecran noir plusieurs cycles au reveil.
  static float smooth = -1.0f;
  smooth = (smooth < 0.0f) ? (float)raw : (smooth * 0.8f + raw * 0.2f);

  // Plancher a 20 : en dessous le panneau est illisible. Bouton de calibration,
  // a remonter si l'ecran reste trop sombre la nuit.
  int brightness = max(int(smooth / 4095.0f * 255.0f), 20);
  this->ledPanel->dma_display->setBrightness8(brightness);
}
