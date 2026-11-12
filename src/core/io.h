#pragma once
// Was die Ablaufsteuerung pro Durchlauf bekommt und zurückgibt.
// Die Hardware-Schicht füllt TickInput, die Steuerung selbst fasst keine Pins an.

#include <stdint.h>

#include "valves.h"

namespace core {

// Ortszeit, bereits aufbereitet (Zeitzone, Sommerzeit). valid = false, solange die Uhr nicht gestellt ist.
struct WallClock {
  bool valid = false;
  uint32_t epoch = 0;        // Unix-Zeit
  uint32_t day = 0;          // fortlaufender Tag in Ortszeit, nur zum Vergleichen
  uint16_t minuteOfDay = 0;  // 0..1439 in Ortszeit
};

struct TickInput {
  uint32_t nowMs = 0;
  WallClock clock;
  bool pressurized = true;         // entprellter Druckschalter
  float flowLpm = 0.0f;            // Durchfluss im Zulauf
  uint32_t msSinceFlowPulse = 0;   // Zeit seit dem letzten Durchfluss-Impuls
};

struct TickOutput {
  ValveMask valves = kAllClosed;  // gewünschte Ventilstellung
  bool saveCounter = false;       // guter Zeitpunkt, den Wasserzähler zu speichern
  uint32_t saveBootEpoch = 0;     // != 0: diesen Startzeitpunkt im NVS speichern
};

}  // namespace core
