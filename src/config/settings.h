#pragma once
// Alle Einstellungen der Anlage mit Standardwerten.
// Ohne Arduino-Abhängigkeit, damit die Steuerlogik testbar bleibt.
// Laden und Speichern im NVS: settings_store.h

#include <stddef.h>
#include <stdint.h>

namespace config {

// Feste Spülzeit. 00:00 ist eine gültige Uhrzeit, "aus" wird über enabled ausgedrückt.
struct FlushTime {
  bool enabled = false;
  uint8_t hour = 0;
  uint8_t minute = 0;
};

constexpr size_t kFlushTimeCount = 2;

struct Settings {
  // Spülplan
  uint8_t systemFlushIntervalH = 0;    // Systemspülung alle x Stunden, 0 = aus
  uint8_t membraneFlushIntervalH = 0;  // Membranspülung alle x Stunden, 0 = aus
  FlushTime flushTimes[kFlushTimeCount];

  // Spüldauern
  uint16_t systemFlushStepS = 60;  // Dauer je Schritt der Systemspülung
  uint16_t membraneFlushS = 300;   // Membranspülung

  // Druckregelung und Sicherheit
  uint16_t pressureBuildMaxS = 300;  // max. Zeit für den Druckaufbau nach einer Entnahme
  uint16_t inletMaxOpenS = 900;      // max. Zeit, die der Zulauf am Stück offen sein darf
  float drawThresholdLpm = 0.0f;     // Durchfluss, ab dem eine Entnahme zählt; 0 = Erkennung aus

  // Durchflusssensor
  float litersPerPulse = 0.00052f;

  // Anzeige: Helligkeit der Status-LED in Prozent, wie das Auge sie wahrnimmt (0 = aus)
  uint8_t statusLedPercent = 10;

  // MQTT
  char mqttServer[64] = "";
  uint16_t mqttPort = 1883;
  char mqttUser[32] = "";
  char mqttPassword[64] = "";
};

// Gültige Bereiche
constexpr uint8_t kMaxFlushIntervalH = 168;  // eine Woche
constexpr uint16_t kMinSystemFlushStepS = 5, kMaxSystemFlushStepS = 1800;
constexpr uint16_t kMinMembraneFlushS = 10, kMaxMembraneFlushS = 3600;
constexpr uint16_t kMinPressureBuildS = 30, kMaxPressureBuildS = 3600;
constexpr uint16_t kMinInletOpenS = 60, kMaxInletOpenS = 7200;
constexpr float kMaxDrawThresholdLpm = 100.0f;
constexpr float kMinLitersPerPulse = 0.00001f, kMaxLitersPerPulse = 1.0f;
constexpr uint8_t kMaxStatusLedPercent = 100;

// Bringt alle Werte in gültige Bereiche und stellt sicher, dass Texte abgeschlossen sind.
// Gibt true zurück, wenn etwas korrigiert werden musste.
bool sanitize(Settings& s);

}  // namespace config
