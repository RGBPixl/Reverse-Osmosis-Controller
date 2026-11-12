#pragma once
// Zeitplan: meldet, wann eine System- oder Membranspülung fällig ist.
// - feste Uhrzeiten (nur mit gültiger Uhrzeit), je Tag höchstens einmal
// - Intervalle (laufen auch ohne Uhrzeit), gezählt ab dem Ende der letzten Spülung desselben Programms

#include <stdint.h>

#include "../config/settings.h"
#include "io.h"
#include "programs.h"

namespace core {

class Scheduler {
 public:
  // So viele Minuten nach der eingestellten Uhrzeit wird eine feste Spülung noch nachgeholt,
  // z. B. wenn die Uhr beim NTP-Abgleich über die Minute springt.
  static constexpr uint16_t kCatchUpMinutes = 2;

  void begin(uint32_t nowMs);

  // Gibt die fälligen Programme als Bitmaske zurück (programBit()).
  // Feste Zeiten werden genau einmal gemeldet, Intervalle so lange, bis programFinished() kommt.
  uint8_t tick(const config::Settings& settings, const WallClock& clock, uint32_t nowMs);

  // Ende (oder Abbruch) einer Spülung: startet das Intervall des Programms neu.
  void programFinished(Program program, uint32_t nowMs);

 private:
  uint32_t lastSystemMs_ = 0;
  uint32_t lastMembraneMs_ = 0;

  struct FixedRun {
    bool ran = false;
    uint32_t day = 0;          // Tag, zu dem die gelaufene Spülung gehört
    uint16_t minuteOfDay = 0;  // eingestellte Uhrzeit, zu der sie lief
  };
  FixedRun fixedRuns_[config::kFlushTimeCount];
};

}  // namespace core
