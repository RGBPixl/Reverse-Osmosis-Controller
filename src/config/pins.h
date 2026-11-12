#pragma once
// Alle Pins der Anlage an einer Stelle.
// Nicht angeschlossene Teile sind als reserviert markiert, siehe docs/spaetere-features.md.

#include <stdint.h>

namespace pins {

// Relais (HIGH = Ventil offen)
constexpr uint8_t kRelayFreshWater = 12;      // Frischwasserzulauf
constexpr uint8_t kRelayWasteWater = 27;      // Abwasser
constexpr uint8_t kRelayMembraneBypass = 26;  // Membran-Bypass

// Reserviert, aktuell nicht angeschlossen. Werden beim Start nur sicher auf LOW gesetzt.
constexpr uint8_t kRelayContainer = 25;
constexpr uint8_t kRelayReserve1 = 33;
constexpr uint8_t kRelayReserve2 = 32;
constexpr uint8_t kFloatSensor = 36;

// Sensoren
constexpr uint8_t kPressureSwitch = 35;  // HIGH = Druck da. Reiner Eingang, externer Widerstand.
constexpr uint8_t kFlowSensor = 34;      // Durchfluss im Zulauf, Impuls an fallender Flanke. Reiner Eingang.
constexpr uint8_t kOneWireBus = 4;       // 2x DS18B20: Index 0 = vor Filter, Index 1 = nach Filter

// Taster (LOW = gedrückt, interner Pull-up)
constexpr uint8_t kButtonOk = 2;
constexpr uint8_t kButtonLeft = 15;
constexpr uint8_t kButtonRight = 0;

// Anzeigen
constexpr uint8_t kLedRing = 23;  // WS2812B
constexpr uint8_t kLedRingCount = 8;
constexpr uint8_t kStatusLedRed = 18;
constexpr uint8_t kStatusLedGreen = 19;
constexpr uint8_t kStatusLedBlue = 14;

// LCD 16x2 über I2C (Standardpins des ESP32)
constexpr uint8_t kI2cSda = 21;
constexpr uint8_t kI2cScl = 22;
constexpr uint8_t kLcdAddress = 0x27;

}  // namespace pins
