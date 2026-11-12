#pragma once
// Spülprogramme als Tabellen: Jeder Schritt sagt, welche Ventile offen sind und wie lange.

#include <stdint.h>

#include "../config/settings.h"
#include "valves.h"

namespace core {

enum class Program : uint8_t {
  Startup = 0,   // Startspülung nach dem Booten
  System = 1,    // Systemspülung
  Membrane = 2,  // Membranspülung
};

constexpr uint8_t kProgramCount = 3;

constexpr uint8_t programBit(Program p) { return static_cast<uint8_t>(1u << static_cast<uint8_t>(p)); }

// Woher die Dauer eines Schritts kommt
enum class StepDuration : uint8_t {
  Fixed,            // fester Wert in fixedS
  SystemFlushStep,  // Settings::systemFlushStepS
  MembraneFlush,    // Settings::membraneFlushS
};

struct Step {
  ValveMask valves;
  StepDuration duration;
  uint16_t fixedS;
};

struct ProgramDef {
  const char* name;
  const Step* steps;
  uint8_t stepCount;
};

const ProgramDef& programDef(Program p);
inline const char* programName(Program p) { return programDef(p).name; }

uint32_t stepDurationMs(const Step& step, const config::Settings& settings);

}  // namespace core
