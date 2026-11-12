#include "controller.h"

#include <stdarg.h>
#include <stdio.h>

#include <initializer_list>

namespace core {

namespace {

constexpr ValveMask kFresh = valveBit(Valve::FreshWater);

constexpr Program kStartOrder[] = {Program::Startup, Program::System, Program::Membrane};

uint8_t index(Program p) { return static_cast<uint8_t>(p); }

}  // namespace

void Controller::begin(const config::Settings& settings, uint32_t nowMs, uint32_t lastBootEpoch) {
  settings_ = settings;
  nowMs_ = beginMs_ = nowMs;
  lastBootEpoch_ = lastBootEpoch;
  scheduler_.begin(nowMs);

  mode_ = Mode::Idle;
  valves_ = manualValves_ = kAllClosed;
  valvesChangedMs_ = freshClosedMs_ = freshOpenedMs_ = nowMs;
  for (bool& p : pending_) {
    p = false;
  }
  fault_ = FaultReason::None;
  warnings_ = 0;
  leakCandidate_ = false;
  startupDecisionPending_ = true;
  bootEpochSaved_ = false;
  abortRequested_ = ackRequested_ = false;
  serviceRequest_ = ServiceRequest::None;
}

TickOutput Controller::tick(const TickInput& in) {
  nowMs_ = in.nowMs;
  TickOutput out;

  handleStartup(in, out);
  handleSchedule(in);
  handleRequests(out);

  switch (mode_) {
    case Mode::Idle: tickIdle(in); break;
    case Mode::Producing: tickProducing(in); break;
    case Mode::Flushing: tickFlushing(out); break;
    case Mode::Service: tickService(); break;
    case Mode::Fault: break;
  }

  updateWarnings(in);
  out.valves = valves_;
  return out;
}

bool Controller::handle(const Command& command) {
  switch (command.type) {
    case CommandType::StartFlush:
      if (mode_ == Mode::Fault) {
        log("%s abgelehnt: Fehler aktiv", programName(command.program));
        return false;
      }
      if (mode_ == Mode::Flushing && program_ == command.program) {
        return false;  // laeuft bereits
      }
      markPending(command.program);
      return true;

    case CommandType::AbortFlush:
      if (mode_ != Mode::Flushing) {
        return false;
      }
      abortRequested_ = true;
      return true;

    case CommandType::SetServiceMode:
      if (command.on) {
        if (mode_ == Mode::Fault || mode_ == Mode::Service) {
          return false;
        }
        manualValves_ = kAllClosed;
        serviceRequest_ = ServiceRequest::Enter;
      } else {
        if (mode_ != Mode::Service && serviceRequest_ != ServiceRequest::Enter) {
          return false;
        }
        serviceRequest_ = ServiceRequest::Leave;
      }
      return true;

    case CommandType::SetValve:
      if (mode_ != Mode::Service && serviceRequest_ != ServiceRequest::Enter) {
        return false;
      }
      if (command.on) {
        manualValves_ = static_cast<ValveMask>(manualValves_ | valveBit(command.valve));
      } else {
        manualValves_ = static_cast<ValveMask>(manualValves_ & ~valveBit(command.valve));
      }
      return true;

    case CommandType::AcknowledgeFault:
      if (mode_ != Mode::Fault) {
        return false;
      }
      ackRequested_ = true;
      return true;
  }
  return false;
}

ControllerStatus Controller::status() const {
  ControllerStatus s;
  s.mode = mode_;
  s.valves = valves_;
  s.fault = fault_;
  s.warnings = warnings_;
  s.startupDecisionPending = startupDecisionPending_;
  for (uint8_t i = 0; i < kProgramCount; ++i) {
    s.pending[i] = pending_[i];
  }
  s.inletOpenMs = isOpen(valves_, Valve::FreshWater) ? elapsed(freshOpenedMs_) : 0;

  if (mode_ == Mode::Flushing) {
    const ProgramDef& def = programDef(program_);
    s.program = program_;
    s.step = step_;
    s.stepCount = def.stepCount;
    const uint32_t stepMs = stepDurationMs(def.steps[step_], settings_);
    const uint32_t inStep = elapsed(stepStartMs_);
    uint32_t remaining = inStep < stepMs ? stepMs - inStep : 0;
    for (uint8_t i = step_ + 1; i < def.stepCount; ++i) {
      remaining += stepDurationMs(def.steps[i], settings_);
    }
    s.programRemainingMs = remaining;
  }
  return s;
}

// ---------------------------------------------------------------------------
// Ablauf pro tick()

void Controller::handleStartup(const TickInput& in, TickOutput& out) {
  if (!bootEpochSaved_ && in.clock.valid) {
    out.saveBootEpoch = in.clock.epoch;
    bootEpochSaved_ = true;
  }
  if (!startupDecisionPending_) {
    return;
  }

  if (in.clock.valid) {
    startupDecisionPending_ = false;
    const bool recent = lastBootEpoch_ != 0 && in.clock.epoch >= lastBootEpoch_ &&
                        in.clock.epoch - lastBootEpoch_ < kStartupMinGapS;
    if (recent) {
      log("Letzter Start vor %lu min, keine Startspuelung",
          static_cast<unsigned long>((in.clock.epoch - lastBootEpoch_) / 60));
      return;
    }
    markPending(Program::Startup);
  } else if (elapsed(beginMs_) >= kStartupClockWaitMs) {
    startupDecisionPending_ = false;
    log("Keine Uhrzeit, Startspuelung laeuft trotzdem");
    markPending(Program::Startup);
  }
}

void Controller::handleSchedule(const TickInput& in) {
  const uint8_t due = scheduler_.tick(settings_, in.clock, nowMs_);
  if (due == 0 || mode_ == Mode::Fault) {
    return;
  }
  for (Program p : {Program::System, Program::Membrane}) {
    if ((due & programBit(p)) == 0) {
      continue;
    }
    if (mode_ == Mode::Flushing && program_ == p) {
      continue;  // laeuft bereits
    }
    markPending(p);
  }
}

void Controller::handleRequests(TickOutput& out) {
  if (ackRequested_) {
    ackRequested_ = false;
    if (mode_ == Mode::Fault) {
      log("Fehler quittiert");
      fault_ = FaultReason::None;
      enterIdle();
    }
  }

  const ServiceRequest request = serviceRequest_;
  serviceRequest_ = ServiceRequest::None;
  if (request == ServiceRequest::Enter && mode_ != Mode::Service && mode_ != Mode::Fault) {
    if (mode_ == Mode::Flushing) {
      finishProgram(false, out);
    }
    mode_ = Mode::Service;
    setValves(kAllClosed);
    log("Service-Modus an, Automatik pausiert");
  } else if (request == ServiceRequest::Leave && mode_ == Mode::Service) {
    manualValves_ = kAllClosed;
    log("Service-Modus aus");
    enterIdle();
  }

  if (abortRequested_) {
    abortRequested_ = false;
    if (mode_ == Mode::Flushing) {
      finishProgram(false, out);
    }
  }
}

void Controller::tickIdle(const TickInput& in) {
  if (startNextPending()) {
    return;
  }
  if (!in.pressurized && valveHoldElapsed()) {
    log("Druck weg, Zulauf auf");
    enterProducing();
  }
}

void Controller::tickProducing(const TickInput& in) {
  if (startNextPending()) {
    return;  // Spuelung unterbricht die Produktion
  }
  if (in.pressurized && valveHoldElapsed()) {
    log("Druck da, Zulauf zu");
    enterIdle();
    return;
  }

  // Entnahme-Erkennung: solange viel fliesst, wird entnommen. Danach muss der Druck
  // innerhalb von pressureBuildMaxS kommen.
  const float threshold = settings_.drawThresholdLpm;
  if (threshold > 0.0f) {
    if (in.flowLpm >= threshold) {
      lastDrawMs_ = nowMs_;
    }
    if (elapsed(lastDrawMs_) >= settings_.pressureBuildMaxS * 1000UL) {
      enterFault(FaultReason::PressureBuildTimeout);
      return;
    }
  }

  if (elapsed(freshOpenedMs_) >= settings_.inletMaxOpenS * 1000UL) {
    enterFault(FaultReason::InletOpenTooLong);
  }
}

void Controller::tickFlushing(TickOutput& out) {
  const ProgramDef& def = programDef(program_);
  if (elapsed(stepStartMs_) < stepDurationMs(def.steps[step_], settings_)) {
    return;
  }
  ++step_;
  if (step_ >= def.stepCount) {
    finishProgram(true, out);
    return;
  }
  stepStartMs_ = nowMs_;
  setValves(def.steps[step_].valves);
}

void Controller::tickService() {
  setValves(manualValves_);
  // Auch von Hand darf der Zulauf nicht unbegrenzt offen bleiben.
  if (isOpen(valves_, Valve::FreshWater) && elapsed(freshOpenedMs_) >= settings_.inletMaxOpenS * 1000UL) {
    enterFault(FaultReason::InletOpenTooLong);
  }
}

void Controller::updateWarnings(const TickInput& in) {
  const uint8_t before = warnings_;

  if (isOpen(valves_, Valve::FreshWater)) {
    leakCandidate_ = false;
    warnings_ = static_cast<uint8_t>(warnings_ & ~kWarnValveLeak);
    const bool noFlow = elapsed(freshOpenedMs_) >= kNoFlowWarnMs && in.msSinceFlowPulse >= kNoFlowWarnMs;
    if (noFlow) {
      warnings_ |= kWarnNoInletFlow;
    } else {
      warnings_ = static_cast<uint8_t>(warnings_ & ~kWarnNoInletFlow);
    }
  } else {
    warnings_ = static_cast<uint8_t>(warnings_ & ~kWarnNoInletFlow);
    const bool flowing = elapsed(freshClosedMs_) >= kMinValveHoldMs && in.flowLpm > kLeakMinLpm;
    if (!flowing) {
      leakCandidate_ = false;
      warnings_ = static_cast<uint8_t>(warnings_ & ~kWarnValveLeak);
    } else if (!leakCandidate_) {
      leakCandidate_ = true;
      leakSinceMs_ = nowMs_;
    } else if (elapsed(leakSinceMs_) >= kLeakWarnMs) {
      warnings_ |= kWarnValveLeak;
    }
  }

  const uint8_t raised = static_cast<uint8_t>(warnings_ & ~before);
  const uint8_t cleared = static_cast<uint8_t>(before & ~warnings_);
  if (raised & kWarnNoInletFlow) log("Warnung: Zulauf offen, aber kein Durchfluss");
  if (raised & kWarnValveLeak) log("Warnung: Zulauf zu, aber Durchfluss (Ventil undicht?)");
  if (cleared & kWarnNoInletFlow) log("Warnung aufgehoben: Durchfluss wieder da");
  if (cleared & kWarnValveLeak) log("Warnung aufgehoben: kein Durchfluss mehr bei geschlossenem Zulauf");
}

// ---------------------------------------------------------------------------
// Zustandswechsel

void Controller::enterIdle() {
  mode_ = Mode::Idle;
  setValves(kAllClosed);
}

void Controller::enterProducing() {
  mode_ = Mode::Producing;
  lastDrawMs_ = nowMs_;
  setValves(kFresh);
}

void Controller::enterFault(FaultReason reason) {
  mode_ = Mode::Fault;
  fault_ = reason;
  manualValves_ = kAllClosed;
  for (bool& p : pending_) {
    p = false;
  }
  setValves(kAllClosed);
  log("FEHLER: %s. Alle Ventile zu, Quittierung noetig", faultText(reason));
}

bool Controller::startNextPending() {
  for (Program p : kStartOrder) {
    if (pending_[index(p)]) {
      pending_[index(p)] = false;
      startProgram(p);
      return true;
    }
  }
  return false;
}

void Controller::startProgram(Program program) {
  const ProgramDef& def = programDef(program);
  mode_ = Mode::Flushing;
  program_ = program;
  step_ = 0;
  stepStartMs_ = nowMs_;
  setValves(def.steps[0].valves);
  log("%s gestartet", def.name);
}

void Controller::finishProgram(bool completed, TickOutput& out) {
  scheduler_.programFinished(program_, nowMs_);
  log("%s %s", programName(program_), completed ? "beendet" : "abgebrochen");
  out.saveCounter = true;
  enterIdle();
}

void Controller::markPending(Program program) {
  if (pending_[index(program)]) {
    return;
  }
  pending_[index(program)] = true;
  const bool busy = mode_ == Mode::Flushing || mode_ == Mode::Service;
  log("%s %s", programName(program), busy ? "vorgemerkt" : "angefordert");
}

void Controller::setValves(ValveMask mask) {
  if (mask == valves_) {
    return;
  }
  const bool freshBefore = isOpen(valves_, Valve::FreshWater);
  const bool freshAfter = isOpen(mask, Valve::FreshWater);
  if (!freshBefore && freshAfter) {
    freshOpenedMs_ = nowMs_;
  } else if (freshBefore && !freshAfter) {
    freshClosedMs_ = nowMs_;
  }
  valves_ = mask;
  valvesChangedMs_ = nowMs_;
}

void Controller::log(const char* fmt, ...) {
  if (logSink_ == nullptr) {
    return;
  }
  char message[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);
  logSink_(message);
}

}  // namespace core
