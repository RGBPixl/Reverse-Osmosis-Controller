#pragma once
// Schaltet die Ventil-Relais. Wird nur von der Ablaufsteuerung benutzt.

#include "../core/valves.h"

namespace hal {

class Relays {
 public:
  // Setzt alle Relais-Pins (auch die reservierten) als Ausgang auf LOW = alle Ventile zu.
  void begin();

  // Stellt alle Ventile gemäß Maske. Nur tatsächliche Änderungen werden geschaltet und geloggt.
  void apply(core::ValveMask mask);

  // Zuletzt gesetzter Zustand (nicht vom Pin gelesen).
  core::ValveMask mask() const { return mask_; }
  bool isOpen(core::Valve v) const { return core::isOpen(mask_, v); }

 private:
  core::ValveMask mask_ = core::kAllClosed;
};

}  // namespace hal
