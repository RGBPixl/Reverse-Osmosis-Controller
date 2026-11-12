#pragma once
// Befehle an die Ablaufsteuerung. Menü, MQTT und Webpanel schalten nie selbst Relais,
// sondern schicken einen dieser Befehle. Die Steuerung entscheidet, ob er gerade erlaubt ist.

#include <stdint.h>

#include "programs.h"
#include "valves.h"

namespace core {

enum class CommandType : uint8_t {
  StartFlush,        // program
  AbortFlush,
  SetServiceMode,    // on
  SetValve,          // valve, on (nur im Service-Modus)
  AcknowledgeFault,
};

struct Command {
  CommandType type;
  Program program;
  Valve valve;
  bool on;

  static Command startFlush(Program p) { return {CommandType::StartFlush, p, Valve::FreshWater, false}; }
  static Command abortFlush() { return {CommandType::AbortFlush, Program::System, Valve::FreshWater, false}; }
  static Command serviceMode(bool on) { return {CommandType::SetServiceMode, Program::System, Valve::FreshWater, on}; }
  static Command setValve(Valve v, bool open) { return {CommandType::SetValve, Program::System, v, open}; }
  static Command acknowledgeFault() { return {CommandType::AcknowledgeFault, Program::System, Valve::FreshWater, false}; }
};

}  // namespace core
