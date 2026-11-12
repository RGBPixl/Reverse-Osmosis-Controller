#include "leds.h"

#include <Arduino.h>

// FastLED-Hinweise beim Übersetzen unterdrücken
#define FASTLED_INTERNAL
#include <FastLED.h>

#include "../config/pins.h"
#include "../util/log.h"

namespace ui {

namespace {

CRGB gRing[pins::kLedRingCount];

constexpr uint32_t kSpinStepMs = 100;
constexpr uint32_t kBlinkMs = 500;

bool blinkOn(uint32_t nowMs) { return (nowMs / kBlinkMs) % 2 == 0; }

void fillRing(const CRGB& color) { fill_solid(gRing, pins::kLedRingCount, color); }

// Ein heller Punkt läuft im Kreis, dahinter ein schwächer werdender Schweif.
void spin(const CRGB& color, uint32_t nowMs) {
  const uint8_t head = static_cast<uint8_t>((nowMs / kSpinStepMs) % pins::kLedRingCount);
  for (uint8_t i = 0; i < pins::kLedRingCount; ++i) {
    const uint8_t distance = static_cast<uint8_t>((head + pins::kLedRingCount - i) % pins::kLedRingCount);
    CRGB c = color;
    c.nscale8(distance == 0 ? 255 : (distance < 3 ? static_cast<uint8_t>(96 >> (distance - 1)) : 0));
    gRing[i] = c;
  }
}

}  // namespace

void Leds::begin() {
  FastLED.addLeds<WS2812B, pins::kLedRing, GRB>(gRing, pins::kLedRingCount);
  FastLED.setBrightness(kRingBrightness);
  fillRing(CRGB::Black);
  FastLED.show();

  pinMode(pins::kStatusLedRed, OUTPUT);
  pinMode(pins::kStatusLedGreen, OUTPUT);
  pinMode(pins::kStatusLedBlue, OUTPUT);
  setStatusLed(false, false, false);
  util::logf("LEDs", "Initialisiert");
}

void Leds::startTest(uint32_t nowMs) {
  testActive_ = true;
  testStartMs_ = nowMs;
  util::logf("LEDs", "LED-Test gestartet");
}

void Leds::update(const core::SystemStatus& status, NetworkState network, uint32_t nowMs) {
  if (testActive_ && runTest(nowMs)) {
    return;
  }
  showRing(status, nowMs);
  showStatusLed(network, nowMs);
}

void Leds::showRing(const core::SystemStatus& status, uint32_t nowMs) {
  const core::Mode mode = status.controller.mode;

  // Nur neu senden, wenn sich das Bild ändert (Modus + Animationsschritt).
  uint32_t frameId = 0;
  if (mode == core::Mode::Flushing) frameId = nowMs / kSpinStepMs;
  if (mode == core::Mode::Fault) frameId = nowMs / kBlinkMs;
  const uint32_t hash = (static_cast<uint32_t>(mode) << 28) ^ frameId;
  if (hash == lastRingHash_) {
    return;
  }
  lastRingHash_ = hash;

  switch (mode) {
    case core::Mode::Idle: fillRing(CRGB::Green); break;
    case core::Mode::Producing: fillRing(CRGB::Blue); break;
    case core::Mode::Flushing: spin(CRGB::Blue, nowMs); break;
    case core::Mode::Service: fillRing(CRGB(255, 160, 0)); break;  // gelb
    case core::Mode::Fault: fillRing(blinkOn(nowMs) ? CRGB::Red : CRGB::Black); break;
  }
  FastLED.show();
}

void Leds::showStatusLed(NetworkState network, uint32_t nowMs) {
  switch (network) {
    case NetworkState::Online: setStatusLed(false, true, false); break;
    case NetworkState::WifiOnly: setStatusLed(true, true, false); break;
    case NetworkState::Offline: setStatusLed(true, false, false); break;
    case NetworkState::Portal: setStatusLed(false, false, blinkOn(nowMs)); break;
  }
}

// Ring: rot, grün, blau, Regenbogen. Status-LED gleichzeitig rot, grün, blau, weiß.
bool Leds::runTest(uint32_t nowMs) {
  const uint32_t step = (nowMs - testStartMs_) / kTestStepMs;
  switch (step) {
    case 0:
      fillRing(CRGB::Red);
      setStatusLed(true, false, false);
      break;
    case 1:
      fillRing(CRGB::Green);
      setStatusLed(false, true, false);
      break;
    case 2:
      fillRing(CRGB::Blue);
      setStatusLed(false, false, true);
      break;
    case 3:
      fill_rainbow(gRing, pins::kLedRingCount, 0, 256 / pins::kLedRingCount);
      setStatusLed(true, true, true);
      break;
    default:
      testActive_ = false;
      lastRingHash_ = 0xFFFFFFFF;  // normales Bild sofort neu zeichnen
      util::logf("LEDs", "LED-Test beendet");
      return false;
  }
  FastLED.show();
  return true;
}

void Leds::setStatusLed(bool red, bool green, bool blue) {
  const uint8_t on = kStatusLedActiveHigh ? HIGH : LOW;
  const uint8_t off = kStatusLedActiveHigh ? LOW : HIGH;
  digitalWrite(pins::kStatusLedRed, red ? on : off);
  digitalWrite(pins::kStatusLedGreen, green ? on : off);
  digitalWrite(pins::kStatusLedBlue, blue ? on : off);
}

}  // namespace ui
