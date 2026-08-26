#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

class Buzzer {
  public:
    static void setup();
    static void on();
    static void off();
    // Bloquant (~3 x duree + 200 ms). Appele une fois par changement d'etat de
    // la ventilation, depuis un refresh deja bloquant : pas de quoi sortir
    // l'artillerie non bloquante.
    static void beepbeepbeep(unsigned int beepDurationInMs);
};

#endif
