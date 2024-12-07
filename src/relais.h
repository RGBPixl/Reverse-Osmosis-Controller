#pragma once
#include "esp32-hal-gpio.h"

class Relais {
private:
  uint8_t relaisPin;

public:
  inline Relais(int rPin) : relaisPin(rPin) {
    pinMode(relaisPin, OUTPUT);
  }

  inline ~Relais() { Serial.print("Relais destroyed"); }

  inline void turnOn() { digitalWrite(relaisPin, HIGH); }

  inline void turnOff() { digitalWrite(relaisPin, LOW); }
};