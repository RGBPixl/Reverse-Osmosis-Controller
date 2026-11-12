#pragma once
// Auswerten und Formatieren von MQTT-Nutzlasten und Texten für Home Assistant.
// Ohne Arduino-Abhängigkeit, mit Unit-Tests (test/test_net).

#include <stddef.h>
#include <stdint.h>

#include "../config/settings.h"
#include "../core/status.h"

namespace net {

// Kopiert eine MQTT-Nutzlast (nicht nullterminiert) und entfernt Leerzeichen am Rand.
void copyPayload(const uint8_t* payload, size_t length, char* out, size_t outLen);

// "ON"/"OFF", "1"/"0", "true"/"false", Groß-/Kleinschreibung egal
bool parseOnOff(const char* text, bool& on);

// Button-Druck aus HA ("PRESS") oder "1"/"ON"
bool isPress(const char* text);

// Ganze Stunden 0..maxHours, auch "8.0" (so sendet HA manchmal Zahlen)
bool parseHours(const char* text, uint8_t maxHours, uint8_t& hours);

// "HH:MM", "H:MM" oder "HH:MM:SS" schaltet ein; "", "--:--", "aus", "off" schaltet aus.
// Bei ungültigem Text false, time bleibt unverändert.
bool parseFlushTime(const char* text, config::FlushTime& time);

// "HH:MM" oder "--:--" für aus
void formatFlushTime(const config::FlushTime& time, char* out, size_t len);

// Zustand als Text für HA, z. B. "Spült: Systemspülung"
void stateText(const core::ControllerStatus& status, char* out, size_t len);

// Fehler oder Warnung vorhanden?
bool hasProblem(const core::ControllerStatus& status);

// "OK", "Fehler: ..." oder "Warnung: ..."
void problemText(const core::ControllerStatus& status, char* out, size_t len);

}  // namespace net
