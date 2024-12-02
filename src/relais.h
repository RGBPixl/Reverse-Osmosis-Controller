#include "esp32-hal-gpio.h"
class Relais {
private:
  int relaisPin;

public:
  Relais(int rPin) {
    relaisPin = rPin;
    pinMode(relaisPin, OUTPUT);
  }

  void turnOn() { digitalWrite(relaisPin, HIGH); }

  void turnOff() { digitalWrite(relaisPin, LOW); }
};