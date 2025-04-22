#pragma once

#include "esp32-hal-gpio.h"
#include <Arduino.h>
#include <time.h>

// 🔧 Relais-Pin zu Namen-Zuordnung
inline const char* getRelaisName(uint8_t pin) {
  switch (pin) {
    case 12: return "Frischwasser";
    case 27: return "Abwasser";
    case 26: return "Membran-Bypass";
    case 25: return "Container";
    case 33: return "Reserve 1";
    case 32: return "Reserve 2";
    default: return "Unbekannt";
  }
}

class Relais {
private:
  uint8_t relaisPin;

  void logState(bool state) {
    struct tm timeinfo;
    char timeBuffer[16] = "??:??:??";
    if (getLocalTime(&timeinfo)) {
      strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
    }

    Serial.printf("[Info] [%s] Relais '%s' (Pin %d) -> %s\n",
                  timeBuffer,
                  getRelaisName(relaisPin),
                  relaisPin,
                  state ? "ON" : "OFF");
  }

public:
  inline Relais(int rPin) : relaisPin(rPin) {
    pinMode(relaisPin, OUTPUT);
    digitalWrite(relaisPin, LOW);
  }

  inline ~Relais() {
    Serial.printf("Relais an Pin %d destroyed\n", relaisPin);
  }

  inline void turnOn() {
    digitalWrite(relaisPin, HIGH);
    logState(true);
  }

  inline void turnOff() {
    digitalWrite(relaisPin, LOW);
    logState(false);
  }

  inline bool isOn() const {
    return digitalRead(relaisPin) == HIGH;
  }
};
