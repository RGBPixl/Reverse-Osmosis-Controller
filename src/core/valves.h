#pragma once
// Die drei angeschlossenen Ventile und eine Bitmaske dafür.
// Die Ablaufsteuerung arbeitet nur mit diesen Typen, nicht mit Pins.

#include <stdint.h>

namespace core {

enum class Valve : uint8_t {
  FreshWater = 0,      // Frischwasserzulauf
  WasteWater = 1,      // Abwasser
  MembraneBypass = 2,  // Membran-Bypass
};

constexpr uint8_t kValveCount = 3;

// Bit gesetzt = Ventil offen
using ValveMask = uint8_t;

constexpr ValveMask kAllClosed = 0;

// Bewusst nicht "bit" genannt: Arduino.h definiert ein gleichnamiges Makro.
constexpr ValveMask valveBit(Valve v) { return static_cast<ValveMask>(1u << static_cast<uint8_t>(v)); }

constexpr bool isOpen(ValveMask mask, Valve v) { return (mask & valveBit(v)) != 0; }

inline const char* valveName(Valve v) {
  switch (v) {
    case Valve::FreshWater: return "Frischwasser";
    case Valve::WasteWater: return "Abwasser";
    case Valve::MembraneBypass: return "Membran-Bypass";
  }
  return "?";
}

}  // namespace core
