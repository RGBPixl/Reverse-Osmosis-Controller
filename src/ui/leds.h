#pragma once
// LED-Ring (Betriebszustand) und RGB-Status-LED (Verbindung).
// update() regelmäßig aufrufen. Animationen laufen über die Zeit, nicht über delay().
//
// LED-Ring:  grün = Bereit, blau = Produziert, blau rotierend = Spülung,
//            gelb = Service, rot blinkend = Fehler
// Status-LED: grün = WLAN + MQTT, gelb = nur WLAN, rot = kein WLAN, blau blinkend = Einrichtungsportal

#include <stdint.h>

#include "../core/status.h"

namespace ui {

enum class NetworkState : uint8_t { Offline, WifiOnly, Online, Portal };

class Leds {
 public:
  // Status-LED: true = Pin HIGH schaltet die Farbe ein (gemeinsame Kathode).
  // Leuchtet die LED in der "Gegenfarbe" (z. B. türkis statt rot), auf false stellen.
  static constexpr bool kStatusLedActiveHigh = true;
  static constexpr uint8_t kRingBrightness = 30;
  static constexpr uint32_t kTestStepMs = 1000;

  void begin();
  void update(const core::SystemStatus& status, NetworkState network, uint32_t nowMs);
  void startTest(uint32_t nowMs);

 private:
  void showRing(const core::SystemStatus& status, uint32_t nowMs);
  void showStatusLed(NetworkState network, uint32_t nowMs);
  bool runTest(uint32_t nowMs);
  void setStatusLed(bool red, bool green, bool blue);

  bool testActive_ = false;
  uint32_t testStartMs_ = 0;
  uint32_t lastRingHash_ = 0xFFFFFFFF;
};

}  // namespace ui
