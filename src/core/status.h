#pragma once
// Zustand der Ablaufsteuerung, wie ihn Display, LEDs, MQTT und Webpanel sehen.

#include <stdint.h>

#include "io.h"
#include "programs.h"
#include "valves.h"

namespace core {

enum class Mode : uint8_t {
  Idle,       // Bereit, Druck da, alle Ventile zu
  Producing,  // Druck fehlt, Zulauf offen
  Flushing,   // Spülprogramm läuft (auch die Startspülung)
  Service,    // Automatik pausiert, Ventile von Hand
  Fault,      // Sicherheitsabschaltung, alle Ventile zu, wartet auf Quittierung
};

enum class FaultReason : uint8_t {
  None,
  PressureBuildTimeout,  // nach Ende der Entnahme kein Druck innerhalb der erlaubten Zeit
  InletOpenTooLong,      // Zulauf länger als erlaubt am Stück offen
};

// Warnungen als Bitmaske, führen nicht zur Abschaltung
constexpr uint8_t kWarnNoInletFlow = 1 << 0;  // Zulauf offen, aber kein Durchfluss
constexpr uint8_t kWarnValveLeak = 1 << 1;    // Zulauf zu, aber Durchfluss

struct ControllerStatus {
  Mode mode = Mode::Idle;
  ValveMask valves = kAllClosed;

  // Nur gültig bei Mode::Flushing
  Program program = Program::System;
  uint8_t step = 0;
  uint8_t stepCount = 0;
  uint32_t programRemainingMs = 0;

  bool pending[kProgramCount] = {false, false, false};  // vorgemerkte Spülungen
  bool startupDecisionPending = false;                  // wartet noch auf die Uhrzeit

  FaultReason fault = FaultReason::None;
  uint8_t warnings = 0;
  uint32_t inletOpenMs = 0;  // wie lange der Zulauf schon offen ist, 0 = zu
};

// Messwerte aus der Hardware-Schicht
struct Measurements {
  float tempC[2] = {0.0f, 0.0f};  // 0 = vor Filter, 1 = nach Filter
  bool tempValid[2] = {false, false};
  double totalLiters = 0.0;  // Wassermenge im Zulauf
  float flowLpm = 0.0f;      // Durchfluss im Zulauf
  bool pressurized = false;  // entprellter Druckschalter
};

// Alles, was Display, LEDs und (ab Phase 4) MQTT und Webpanel anzeigen.
struct SystemStatus {
  ControllerStatus controller;
  Measurements measurements;
  WallClock clock;
};

inline const char* modeName(Mode m) {
  switch (m) {
    case Mode::Idle: return "Bereit";
    case Mode::Producing: return "Produziert";
    case Mode::Flushing: return "Spuelt";
    case Mode::Service: return "Service";
    case Mode::Fault: return "Fehler";
  }
  return "?";
}

inline const char* faultText(FaultReason r) {
  switch (r) {
    case FaultReason::None: return "";
    case FaultReason::PressureBuildTimeout: return "Kein Druckaufbau nach Entnahme";
    case FaultReason::InletOpenTooLong: return "Zulauf zu lange offen";
  }
  return "?";
}

}  // namespace core
