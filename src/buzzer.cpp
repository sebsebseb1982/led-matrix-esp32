#include "buzzer.h"

#define pwmChannel 0    // Canal LEDC 0
#define frequence 2000  // Frequence PWM de 2 kHz
#define resolution 8    // Resolution de 8 bits, 256 valeurs possibles

#define BUZZER_PIN 18

void Buzzer::setup() {
  ledcSetup(pwmChannel, frequence, resolution);
  ledcAttachPin(BUZZER_PIN, pwmChannel);
  off();
}

void Buzzer::on()  { ledcWrite(pwmChannel, 255); }
void Buzzer::off() { ledcWrite(pwmChannel, 0); }

void Buzzer::beepbeepbeep(unsigned int beepDurationInMs) {
  for (int i = 0; i < 3; i++) {
    on();
    delay(beepDurationInMs);
    off();
    if (i < 2) delay(100);
  }
}
