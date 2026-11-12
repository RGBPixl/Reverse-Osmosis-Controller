#include "relays.h"

#include <Arduino.h>

#include "../config/pins.h"
#include "../util/log.h"

namespace hal {

namespace {

constexpr uint8_t kValvePins[core::kValveCount] = {
    pins::kRelayFreshWater,      // Valve::FreshWater
    pins::kRelayWasteWater,      // Valve::WasteWater
    pins::kRelayMembraneBypass,  // Valve::MembraneBypass
};

constexpr uint8_t kReservedPins[] = {
    pins::kRelayContainer,
    pins::kRelayReserve1,
    pins::kRelayReserve2,
};

void initOutputLow(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

}  // namespace

void Relays::begin() {
  for (uint8_t pin : kValvePins) {
    initOutputLow(pin);
  }
  for (uint8_t pin : kReservedPins) {
    initOutputLow(pin);
  }
  mask_ = core::kAllClosed;
  util::logf("Ventile", "Initialisiert, alle zu");
}

void Relays::apply(core::ValveMask mask) {
  const core::ValveMask changed = mask ^ mask_;
  if (changed == 0) {
    return;
  }
  for (uint8_t i = 0; i < core::kValveCount; ++i) {
    const auto valve = static_cast<core::Valve>(i);
    if (!core::isOpen(changed, valve)) {
      continue;
    }
    const bool open = core::isOpen(mask, valve);
    digitalWrite(kValvePins[i], open ? HIGH : LOW);
    util::logf("Ventil", "%s -> %s", core::valveName(valve), open ? "auf" : "zu");
  }
  mask_ = mask;
}

}  // namespace hal
