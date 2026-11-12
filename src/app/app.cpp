#include "app.h"

#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "../config/settings_store.h"
#include "../config/version.h"
#include "../util/clock.h"
#include "../util/log.h"

namespace app {

namespace {

void logController(const char* message) { util::logf("Steuerung", "%s", message); }

}  // namespace

void App::setup() {
  // Ventile so früh wie möglich in einen sicheren Zustand bringen.
  relays_.begin();

  util::logf("App", "Reverse-Osmosis-Controller %s startet", config::kFirmwareVersion);

  // Zeitzone schon jetzt setzen, damit eine spätere NTP-Zeit sofort richtig umgerechnet wird.
  setenv("TZ", util::kTimezone, 1);
  tzset();

  settings_ = config::loadSettings();
  const double totalLiters = config::loadTotalLiters();
  const uint32_t lastBoot = config::loadLastBootEpoch();

  const uint32_t now = millis();
  inputs_.begin(now);
  flow_.begin(totalLiters, settings_.litersPerPulse, now);
  temperature_.begin(now);
  display_.begin();
  leds_.begin();

  controller_.setLogSink(logController);
  controller_.begin(settings_, now, lastBoot);

  lastSavedLiters_ = totalLiters;
  lastSaveMs_ = lastTickMs_ = lastDisplayMs_ = lastLedMs_ = now;

  // Gemeinsame Daten anlegen, dann den Netzwerk-Task starten.
  settingsHub_.begin(settings_);
  settingsVersion_ = settingsHub_.version();
  sharedStatus_.begin();
  networkInfo_.begin();
  requests_.begin();
  updateStatus();
  sharedStatus_.set(status_);
  network_.start();

  util::logf("App", "Bereit");
}

void App::loop() {
  const uint32_t now = millis();

  if (now - lastTickMs_ >= kTickMs) {
    lastTickMs_ = now;
    tick(now);
  }
  if (now - lastDisplayMs_ >= kDisplayMs) {
    lastDisplayMs_ = now;
    menu_.render(status_, now, frame_);
    display_.show(frame_);
  }
  if (now - lastLedMs_ >= kLedMs) {
    lastLedMs_ = now;
    leds_.update(status_, networkState(), now);
  }

  delay(1);  // anderen Tasks Zeit geben
}

void App::tick(uint32_t nowMs) {
  inputs_.update(nowMs);
  flow_.update(nowMs);
  temperature_.update(nowMs);
  handleSettingsChanges(nowMs);
  handleRequests();

  input_.nowMs = nowMs;
  input_.clock = readClock();
  input_.pressurized = inputs_.pressurized();
  input_.flowLpm = flow_.litersPerMinute();
  input_.msSinceFlowPulse = flow_.msSinceLastPulse(nowMs);

  const core::TickOutput out = controller_.tick(input_);
  relays_.apply(out.valves);
  persist(out, nowMs);

  updateStatus();
  sharedStatus_.set(status_);
  handleButtons(nowMs);
}

void App::handleRequests() {
  Request request;
  while (requests_.pop(request)) {
    switch (request.type) {
      case RequestType::Controller: controller_.handle(request.command); break;
      case RequestType::ResetCounter: resetWaterCounter(); break;
    }
  }
}

void App::handleSettingsChanges(uint32_t nowMs) {
  uint32_t version = 0;
  if (settingsHub_.version() != settingsVersion_) {
    settings_ = settingsHub_.get(&version);
    settingsVersion_ = version;
    controller_.setSettings(settings_);
    flow_.setLitersPerPulse(settings_.litersPerPulse);
    settingsDirty_ = true;
    settingsChangedMs_ = nowMs;
  }
  if (settingsDirty_ && nowMs - settingsChangedMs_ >= kSettingsSaveDelayMs) {
    settingsDirty_ = !config::saveSettings(settings_);
  }
}

ui::NetworkState App::networkState() const {
  const core::NetworkInfo& n = status_.network;
  if (n.portalActive) return ui::NetworkState::Portal;
  if (!n.wifiConnected) return ui::NetworkState::Offline;
  if (n.mqttConfigured && !n.mqttConnected) return ui::NetworkState::WifiOnly;
  return ui::NetworkState::Online;
}

void App::handleButtons(uint32_t nowMs) {
  struct Mapping {
    hal::Button button;
    ui::Key key;
  };
  static const Mapping kMappings[] = {
      {hal::Button::Ok, ui::Key::Ok},
      {hal::Button::Left, ui::Key::Left},
      {hal::Button::Right, ui::Key::Right},
  };
  for (const Mapping& m : kMappings) {
    const hal::ButtonEvent event = inputs_.takeEvent(m.button);
    if (event == hal::ButtonEvent::None) {
      continue;
    }
    const ui::Press press = event == hal::ButtonEvent::Long ? ui::Press::Long : ui::Press::Short;
    menu_.handleKey(m.key, press, status_, nowMs);
    lastDisplayMs_ = nowMs - kDisplayMs;  // Display sofort aktualisieren
  }
}

void App::persist(const core::TickOutput& out, uint32_t nowMs) {
  if (out.saveBootEpoch != 0) {
    config::saveLastBootEpoch(out.saveBootEpoch);
    util::logf("App", "Startzeitpunkt gespeichert");
  }

  const double liters = flow_.totalLiters();
  const double unsaved = fabs(liters - lastSavedLiters_);
  const bool due = unsaved >= kSaveEveryLiters || (unsaved >= 0.01 && nowMs - lastSaveMs_ >= kSaveEveryMs) ||
                   (out.saveCounter && unsaved >= 0.01);
  if (due) {
    saveCounter(nowMs);
  }
}

void App::saveCounter(uint32_t nowMs) {
  const double liters = flow_.totalLiters();
  if (config::saveTotalLiters(liters)) {
    lastSavedLiters_ = liters;
    lastSaveMs_ = nowMs;
    util::logf("App", "Wasserzaehler gespeichert: %.2f L", liters);
  }
}

void App::updateStatus() {
  status_.controller = controller_.status();
  status_.clock = input_.clock;
  status_.network = networkInfo_.get();

  core::Measurements& m = status_.measurements;
  for (uint8_t i = 0; i < 2; ++i) {
    m.tempValid[i] = temperature_.valid(i);
    m.tempC[i] = temperature_.celsius(i);
  }
  m.totalLiters = flow_.totalLiters();
  m.flowLpm = flow_.litersPerMinute();
  m.pressurized = inputs_.pressurized();
}

core::WallClock App::readClock() const {
  core::WallClock clock;
  const time_t now = time(nullptr);
  if (!util::timeValid(now)) {
    return clock;
  }
  struct tm local;
  localtime_r(&now, &local);
  clock.valid = true;
  clock.epoch = static_cast<uint32_t>(now);
  clock.day = static_cast<uint32_t>(util::daysFromCivil(local.tm_year + 1900, static_cast<uint32_t>(local.tm_mon + 1),
                                                        static_cast<uint32_t>(local.tm_mday)));
  clock.minuteOfDay = static_cast<uint16_t>(local.tm_hour * 60 + local.tm_min);
  return clock;
}

// ---------------------------------------------------------------------------
// ui::MenuHost

bool App::sendCommand(const core::Command& command) { return controller_.handle(command); }

void App::resetWaterCounter() {
  flow_.resetTotal();
  saveCounter(millis());
  util::logf("App", "Wasserzaehler auf 0 gesetzt");
}

void App::factoryReset() {
  config::eraseSettings();
  // Über den Hub, damit auch der Netzwerk-Task (MQTT-Zugang) die Standardwerte bekommt.
  settingsHub_.modify([](config::Settings& s) { s = config::Settings(); });
  util::logf("App", "Werksreset: Standardeinstellungen aktiv");
}

void App::startLedTest() { leds_.startTest(millis()); }

void App::startWifiPortal() { network_.requestPortal(); }

}  // namespace app
