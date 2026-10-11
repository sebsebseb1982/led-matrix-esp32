#include "brightness.h"

// Bornes mesurees sur le capteur en place : ~0-100 capteur cache, ~600 piece
// eclairee, ~2800 sous une lampe. A ajuster si l'ecran est trop sombre ou trop
// lumineux. Plancher a 20 : en dessous le panneau est illisible.
#define RAW_DARK        100
#define RAW_BRIGHT      2800
#define BRIGHTNESS_MIN  20
#define BRIGHTNESS_MAX  255
#define ADC_SAMPLES     16

Brightness::Brightness(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Brightness::loop() {
  // Moyenne de plusieurs lectures : l'ADC de l'ESP32 est bruite, une mesure
  // isolee pouvait varier de 130 a 830 sous le meme eclairage.
  long sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(LIGHT_SENSOR_PIN);
  }
  int raw = sum / ADC_SAMPLES;

  // Lissage exponentiel contre le scintillement ; appele toutes les secondes,
  // l'ecran s'adapte en ~5 s. Premier appel : on part de la mesure, sinon la
  // rampe depuis 0 laisse l'ecran noir au reveil.
  static float smooth = -1.0f;
  smooth = (smooth < 0.0f) ? (float)raw : (smooth * 0.8f + raw * 0.2f);

  float t = (smooth - RAW_DARK) / (float)(RAW_BRIGHT - RAW_DARK);
  t = constrain(t, 0.0f, 1.0f);
  int brightness = BRIGHTNESS_MIN + int(t * (BRIGHTNESS_MAX - BRIGHTNESS_MIN));

  static int last = -1;
  if (brightness != last) {
    Serial.printf("[brightness] raw=%d smooth=%.0f -> %d\n", raw, smooth, brightness);
    this->ledPanel->dma_display->setBrightness8(brightness);
    last = brightness;
  }
}
