#include "programs.h"

namespace core {

namespace {

constexpr ValveMask kFresh = valveBit(Valve::FreshWater);
constexpr ValveMask kWaste = valveBit(Valve::WasteWater);
constexpr ValveMask kBypass = valveBit(Valve::MembraneBypass);

// Entspricht der alten Firmware, ohne die 5 s Wartezeit am Anfang
// (die Steuerung wartet beim Start ohnehin bis zu 60 s auf die Uhrzeit).
constexpr Step kStartupSteps[] = {
    {kFresh, StepDuration::Fixed, 5},
    {kFresh | kBypass, StepDuration::Fixed, 10},
    {kFresh | kWaste, StepDuration::Fixed, 10},
    {kFresh | kBypass, StepDuration::Fixed, 10},
};

constexpr Step kSystemSteps[] = {
    {kFresh, StepDuration::Fixed, 1},
    {kFresh | kBypass, StepDuration::SystemFlushStep, 0},
    {kFresh | kWaste, StepDuration::SystemFlushStep, 0},
    {kFresh | kBypass, StepDuration::SystemFlushStep, 0},
};

constexpr Step kMembraneSteps[] = {
    {kFresh | kWaste | kBypass, StepDuration::MembraneFlush, 0},
};

template <typename T, size_t N>
constexpr uint8_t countOf(const T (&)[N]) {
  return static_cast<uint8_t>(N);
}

// Reihenfolge wie enum Program
const ProgramDef kPrograms[kProgramCount] = {
    {"Startspuelung", kStartupSteps, countOf(kStartupSteps)},
    {"Systemspuelung", kSystemSteps, countOf(kSystemSteps)},
    {"Membranspuelung", kMembraneSteps, countOf(kMembraneSteps)},
};

}  // namespace

const ProgramDef& programDef(Program p) { return kPrograms[static_cast<uint8_t>(p)]; }

uint32_t stepDurationMs(const Step& step, const config::Settings& settings) {
  switch (step.duration) {
    case StepDuration::Fixed: return step.fixedS * 1000UL;
    case StepDuration::SystemFlushStep: return settings.systemFlushStepS * 1000UL;
    case StepDuration::MembraneFlush: return settings.membraneFlushS * 1000UL;
  }
  return 0;
}

}  // namespace core
