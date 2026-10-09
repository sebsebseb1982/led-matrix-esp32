#include "buzzer.h"

#define frequence 2000  // Frequence PWM de 2 kHz
#define resolution 8    // Resolution de 8 bits, 256 valeurs possibles

#define BUZZER_PIN 18

void Buzzer::setup() {
  // API LEDC du core Arduino-ESP32 3.x : le canal est alloue automatiquement
  // et on adresse ensuite le PWM par la broche.
  ledcAttach(BUZZER_PIN, frequence, resolution);
  off();
}

void Buzzer::on()  { ledcWrite(BUZZER_PIN, 255); }
void Buzzer::off() { ledcWrite(BUZZER_PIN, 0); }

void Buzzer::beepbeepbeep(unsigned int beepDurationInMs) {
  for (int i = 0; i < 3; i++) {
    on();
    delay(beepDurationInMs);
    off();
    if (i < 2) delay(100);
  }
}
