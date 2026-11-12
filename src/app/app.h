#pragma once
// Verbindet Hardware, Ablaufsteuerung, Bedienung und Speicher.
// Läuft komplett im Arduino-loop() und blockiert nie.
//
// Ablauf pro Durchlauf (alle 10 ms):
//   Eingänge lesen -> Steuerung.tick() -> Ventile stellen -> Speichern -> Menü/Display/LEDs

#include <stdint.h>

#include "../config/settings.h"
#include "../core/controller.h"
#include "../core/status.h"
#include "../hal/flow_meter.h"
#include "../hal/inputs.h"
#include "../hal/relays.h"
#include "../hal/temperature.h"
#include "../net/network.h"
#include "../ui/display.h"
#include "../ui/leds.h"
#include "../ui/menu.h"
#include "shared.h"

namespace app {

class App : public ui::MenuHost {
 public:
  static constexpr uint32_t kTickMs = 10;
  static constexpr uint32_t kDisplayMs = 200;
  static constexpr uint32_t kLedMs = 20;
  static constexpr double kSaveEveryLiters = 10.0;
  static constexpr uint32_t kSaveEveryMs = 30UL * 60UL * 1000UL;
  static constexpr uint32_t kSettingsSaveDelayMs = 2000;  // mehrere Änderungen kurz hintereinander zusammenfassen

  App() : network_(requests_, settingsHub_, sharedStatus_, networkInfo_), menu_(*this) {}

  void setup();
  void loop();

  // ui::MenuHost
  bool sendCommand(const core::Command& command) override;
  void resetWaterCounter() override;
  void factoryReset() override;
  void startLedTest() override;
  void startWifiPortal() override;

 private:
  void tick(uint32_t nowMs);
  void handleRequests();
  void handleSettingsChanges(uint32_t nowMs);
  void handleButtons(uint32_t nowMs);
  void persist(const core::TickOutput& out, uint32_t nowMs);
  void saveCounter(uint32_t nowMs);
  void updateStatus();
  core::WallClock readClock() const;
  ui::NetworkState networkState() const;

  // Zwischen den Tasks geteilt (siehe shared.h). Vor network_ deklariert, weil network_ sie benutzt.
  SettingsHub settingsHub_;
  Shared<core::SystemStatus> sharedStatus_;
  Shared<core::NetworkInfo> networkInfo_;
  RequestQueue requests_;
  net::Network network_;

  config::Settings settings_;  // zuletzt übernommene Einstellungen
  uint32_t settingsVersion_ = 0;
  bool settingsDirty_ = false;
  uint32_t settingsChangedMs_ = 0;

  hal::Relays relays_;
  hal::Inputs inputs_;
  hal::FlowMeter flow_;
  hal::Temperature temperature_;
  core::Controller controller_;
  ui::Display display_;
  ui::Leds leds_;
  ui::Menu menu_;
  ui::Frame frame_;

  core::SystemStatus status_;
  core::TickInput input_;

  uint32_t lastTickMs_ = 0;
  uint32_t lastDisplayMs_ = 0;
  uint32_t lastLedMs_ = 0;
  double lastSavedLiters_ = 0.0;
  uint32_t lastSaveMs_ = 0;
};

}  // namespace app
