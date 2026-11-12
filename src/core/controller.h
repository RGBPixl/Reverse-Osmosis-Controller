#pragma once
// Ablaufsteuerung: der einzige Teil der Firmware, der über Ventilstellungen entscheidet.
//
// - tick() wird regelmäßig aufgerufen, blockiert nie und gibt die gewünschten Ventile zurück.
// - handle() nimmt Befehle von Menü, MQTT und Webpanel an. Ausgeführt werden sie im nächsten tick().
// - Ohne Arduino-Abhängigkeit, dadurch mit Unit-Tests auf dem Mac prüfbar (test/test_core).
//
// Verhalten im Detail: docs/refactoring-konzept.md

#include <stdint.h>

#include "../config/settings.h"
#include "commands.h"
#include "io.h"
#include "log_sink.h"
#include "programs.h"
#include "scheduler.h"
#include "status.h"
#include "valves.h"

namespace core {

class Controller {
 public:
  static constexpr uint32_t kMinValveHoldMs = 10000;      // Mindestzeit zwischen Zulauf auf/zu
  static constexpr uint32_t kStartupClockWaitMs = 60000;  // so lange auf die Uhrzeit warten
  static constexpr uint32_t kStartupMinGapS = 3600;       // Startspülung nur, wenn letzter Start länger her
  static constexpr uint32_t kNoFlowWarnMs = 60000;        // Zulauf offen ohne Durchfluss -> Warnung
  static constexpr uint32_t kLeakWarnMs = 30000;          // Zulauf zu mit Durchfluss -> Warnung
  static constexpr float kLeakMinLpm = 0.1f;              // darunter gilt der Durchfluss als 0

  // lastBootEpoch: gespeicherter Zeitpunkt des vorigen Starts, 0 = unbekannt
  void begin(const config::Settings& settings, uint32_t nowMs, uint32_t lastBootEpoch);

  void setSettings(const config::Settings& settings) { settings_ = settings; }
  const config::Settings& settings() const { return settings_; }
  void setLogSink(LogSink sink) { logSink_ = sink; }

  TickOutput tick(const TickInput& in);

  // true = Befehl angenommen
  bool handle(const Command& command);

  ControllerStatus status() const;

 private:
  enum class ServiceRequest : uint8_t { None, Enter, Leave };

  void handleStartup(const TickInput& in, TickOutput& out);
  void handleSchedule(const TickInput& in);
  void handleRequests(TickOutput& out);
  void tickIdle(const TickInput& in);
  void tickProducing(const TickInput& in);
  void tickFlushing(TickOutput& out);
  void tickService();
  void updateWarnings(const TickInput& in);

  void enterIdle();
  void enterProducing();
  void enterFault(FaultReason reason);
  bool startNextPending();
  void startProgram(Program program);
  void finishProgram(bool completed, TickOutput& out);
  void markPending(Program program);

  void setValves(ValveMask mask);
  uint32_t elapsed(uint32_t sinceMs) const { return nowMs_ - sinceMs; }
  bool valveHoldElapsed() const { return elapsed(valvesChangedMs_) >= kMinValveHoldMs; }
  void log(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

  config::Settings settings_;
  Scheduler scheduler_;
  LogSink logSink_ = nullptr;

  Mode mode_ = Mode::Idle;
  ValveMask valves_ = kAllClosed;
  uint32_t nowMs_ = 0;
  uint32_t valvesChangedMs_ = 0;
  uint32_t freshOpenedMs_ = 0;  // seit wann der Zulauf offen ist
  uint32_t freshClosedMs_ = 0;  // seit wann der Zulauf zu ist

  // Spülung
  Program program_ = Program::System;
  uint8_t step_ = 0;
  uint32_t stepStartMs_ = 0;
  bool pending_[kProgramCount] = {false, false, false};

  // Druckregelung
  uint32_t lastDrawMs_ = 0;  // zuletzt Durchfluss über der Entnahme-Schwelle

  // Service-Modus
  ValveMask manualValves_ = kAllClosed;

  // Fehler und Warnungen
  FaultReason fault_ = FaultReason::None;
  uint8_t warnings_ = 0;
  bool leakCandidate_ = false;
  uint32_t leakSinceMs_ = 0;

  // Start
  uint32_t beginMs_ = 0;
  uint32_t lastBootEpoch_ = 0;
  bool startupDecisionPending_ = true;
  bool bootEpochSaved_ = false;

  // Angenommene Befehle, die der nächste tick() ausführt
  bool abortRequested_ = false;
  bool ackRequested_ = false;
  ServiceRequest serviceRequest_ = ServiceRequest::None;
};

}  // namespace core
