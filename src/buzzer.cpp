#include "buzzer.h"

#define pwmChannel 0    //Choisit le canal 0
#define frequence 2000  //Fréquence PWM de 1 KHz
#define resolution 8    // Résolution de 8 bits, 256 valeurs possibles

#define BUZZER_PIN 0

unsigned long Buzzer::stopBeepMillis = 0;
unsigned int Buzzer::currentLevel = 0;

void Buzzer::setup() {
  ledcAttach(BUZZER_PIN, frequence, resolution);
  ledcWrite(pwmChannel, 255);
  //pinMode(BUZZER_PIN, OUTPUT);
}

void Buzzer::on() {
  ledcWrite(
    pwmChannel,
    255
    //    currentLevel
  );

  //digitalWrite(BUZZER_PIN, HIGH);
}

void Buzzer::off() {
  ledcWrite(
    pwmChannel,
    0);

  digitalWrite(BUZZER_PIN, LOW);
}

void Buzzer::beep(unsigned int durationInMs) {
  on();
  Buzzer::stopBeepMillis = millis() + durationInMs;
}

void Buzzer::loop() {
  /*boolean isNight = Clock::time < "09:00" || Clock::time > "20:00";
  if (isNight) {
    currentLevel = 3;
  } else {
    currentLevel = 255;
  }*/

  if (millis() > stopBeepMillis) {
    off();
  }
}

void Buzzer::beepbeepbeep(unsigned int beepDurationInMs) {
  Buzzer::on();
  delay(beepDurationInMs);
  Buzzer::off();
  delay(100);
  Buzzer::on();
  delay(beepDurationInMs);
  Buzzer::off();
  delay(100);
  Buzzer::on();
  delay(beepDurationInMs);
  Buzzer::off();
}