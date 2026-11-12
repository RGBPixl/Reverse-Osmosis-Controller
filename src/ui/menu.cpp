#include "menu.h"

#include <stdio.h>

#include "../config/network.h"
#include "../config/version.h"
#include "../util/clock.h"

namespace ui {

using core::Command;
using core::Mode;
using core::Program;
using core::SystemStatus;
using core::Valve;

namespace {

// ---------------------------------------------------------------------------
// Texte

const char* faultShort(core::FaultReason reason) {
  switch (reason) {
    case core::FaultReason::None: return "";
    case core::FaultReason::PressureBuildTimeout: return "Kein Druckaufbau";
    case core::FaultReason::InletOpenTooLong: return "Zulauf zu lange";
  }
  return "?";
}

const char* programShort(Program p) {
  switch (p) {
    case Program::Startup: return "Start";
    case Program::System: return "System";
    case Program::Membrane: return "Membran";
  }
  return "?";
}

const char* modeLabel(Mode m) {
  switch (m) {
    case Mode::Idle: return "Bereit";
    case Mode::Producing: return "Produziert";
    case Mode::Flushing: return "Spülung";
    case Mode::Service: return "Service";
    case Mode::Fault: return "FEHLER";
  }
  return "?";
}

void formatMinSec(uint32_t ms, char* out, size_t len) {
  const uint32_t s = (ms + 999) / 1000;
  snprintf(out, len, "%lu:%02lu", static_cast<unsigned long>(s / 60), static_cast<unsigned long>(s % 60));
}

void formatValves(core::ValveMask v, char* out, size_t len) {
  const char* fresh = core::isOpen(v, Valve::FreshWater) ? "auf" : "zu";
  const char* waste = core::isOpen(v, Valve::WasteWater) ? "auf" : "zu";
  const char* bypass = core::isOpen(v, Valve::MembraneBypass) ? "auf" : "zu";
  const int written = snprintf(out, len, "Z:%s A:%s B:%s", fresh, waste, bypass);
  if (written > kCols) {
    snprintf(out, len, "Z:%sA:%sB:%s", fresh, waste, bypass);  // alle offen: ohne Leerzeichen
  }
}

void formatLiters(double liters, char* out, size_t len) {
  if (liters < 10000.0) {
    snprintf(out, len, "%.1f L", liters);
  } else {
    snprintf(out, len, "%.0f L", liters);
  }
}

bool alternate(uint32_t nowMs) { return (nowMs / Menu::kAlternateMs) % 2 == 1; }

char wifiIcon(const core::NetworkInfo& n) {
  if (n.portalActive) return 'P';
  if (!n.wifiConnected) return kCharWifiOff;
  if (n.rssi > -60) return kCharWifi3;
  if (n.rssi > -70) return kCharWifi2;
  if (n.rssi > -80) return kCharWifi1;
  return kCharWifi0;
}

// ---------------------------------------------------------------------------
// Menüeinträge

enum class Action : uint8_t {
  None,
  StartSystem,
  StartMembrane,
  AbortFlush,
  AckFault,
  ToggleService,
  ToggleFresh,
  ToggleWaste,
  ToggleBypass,
  ResetCounter,
  LedTest,
  FactoryReset,
  WifiPortal,
};

using ValueFn = void (*)(const SystemStatus& s, uint32_t nowMs, char* out, size_t len);
using VisibleFn = bool (*)(const SystemStatus& s);

struct Item {
  const char* title;
  ValueFn value;
  Action action;
  bool confirm;
  VisibleFn visible;  // nullptr = immer sichtbar
};

struct Category {
  const char* title;
  const Item* items;
  uint8_t count;
};

void valueTemp(const SystemStatus& s, uint8_t index, char* out, size_t len) {
  if (s.measurements.tempValid[index]) {
    snprintf(out, len, "%.1f °C", static_cast<double>(s.measurements.tempC[index]));
  } else {
    snprintf(out, len, "kein Sensor");
  }
}
void valueTempBefore(const SystemStatus& s, uint32_t, char* out, size_t len) { valueTemp(s, 0, out, len); }
void valueTempAfter(const SystemStatus& s, uint32_t, char* out, size_t len) { valueTemp(s, 1, out, len); }
void valueWater(const SystemStatus& s, uint32_t, char* out, size_t len) {
  formatLiters(s.measurements.totalLiters, out, len);
}
void valueFlow(const SystemStatus& s, uint32_t, char* out, size_t len) {
  snprintf(out, len, "%.2f L/min", static_cast<double>(s.measurements.flowLpm));
}
void valuePressure(const SystemStatus& s, uint32_t, char* out, size_t len) {
  snprintf(out, len, "%s", s.measurements.pressurized ? "Druck da" : "kein Druck");
}
void valueValves(const SystemStatus& s, uint32_t, char* out, size_t len) {
  formatValves(s.controller.valves, out, len);
}
void valueClock(const SystemStatus& s, uint32_t, char* out, size_t len) {
  if (!s.clock.valid) {
    snprintf(out, len, "nicht gestellt");
    return;
  }
  const util::CivilDate d = util::civilFromDays(static_cast<int32_t>(s.clock.day));
  snprintf(out, len, "%02u.%02u.%02u %02u:%02u", d.day, d.month, static_cast<unsigned>(d.year % 100),
           s.clock.minuteOfDay / 60u, s.clock.minuteOfDay % 60u);
}

void valueFlush(const SystemStatus& s, Program p, char* out, size_t len) {
  const core::ControllerStatus& c = s.controller;
  if (c.mode == Mode::Flushing && c.program == p) {
    char remaining[8];
    formatMinSec(c.programRemainingMs, remaining, sizeof(remaining));
    snprintf(out, len, "läuft %s", remaining);
  } else if (c.pending[static_cast<uint8_t>(p)]) {
    snprintf(out, len, "vorgemerkt");
  } else {
    snprintf(out, len, "OK = Starten");
  }
}
void valueSystemFlush(const SystemStatus& s, uint32_t, char* out, size_t len) {
  valueFlush(s, Program::System, out, len);
}
void valueMembraneFlush(const SystemStatus& s, uint32_t, char* out, size_t len) {
  valueFlush(s, Program::Membrane, out, len);
}
void valueAbort(const SystemStatus&, uint32_t, char* out, size_t len) { snprintf(out, len, "OK = Abbrechen"); }
void valueFault(const SystemStatus& s, uint32_t nowMs, char* out, size_t len) {
  snprintf(out, len, "%s", alternate(nowMs) ? "OK = Quittieren" : faultShort(s.controller.fault));
}
void valueService(const SystemStatus& s, uint32_t, char* out, size_t len) {
  snprintf(out, len, "%s", s.controller.mode == Mode::Service ? "an (OK = aus)" : "aus (OK = an)");
}
void valueValve(const SystemStatus& s, Valve v, char* out, size_t len) {
  snprintf(out, len, "%s", core::isOpen(s.controller.valves, v) ? "auf (OK = zu)" : "zu (OK = auf)");
}
void valueFresh(const SystemStatus& s, uint32_t, char* out, size_t len) { valueValve(s, Valve::FreshWater, out, len); }
void valueWaste(const SystemStatus& s, uint32_t, char* out, size_t len) { valueValve(s, Valve::WasteWater, out, len); }
void valueBypass(const SystemStatus& s, uint32_t, char* out, size_t len) {
  valueValve(s, Valve::MembraneBypass, out, len);
}
void valueResetCounter(const SystemStatus&, uint32_t, char* out, size_t len) { snprintf(out, len, "OK = auf 0"); }
void valueStart(const SystemStatus&, uint32_t, char* out, size_t len) { snprintf(out, len, "OK = Starten"); }
void valueFactoryReset(const SystemStatus&, uint32_t, char* out, size_t len) { snprintf(out, len, "OK = Ausführen"); }
void valueVersion(const SystemStatus&, uint32_t, char* out, size_t len) {
  snprintf(out, len, "%s", config::kFirmwareVersion);
}
void valueWifi(const SystemStatus& s, uint32_t, char* out, size_t len) {
  if (s.network.portalActive) {
    snprintf(out, len, "Portal: %s", config::kSetupApName);
  } else if (s.network.wifiConnected) {
    snprintf(out, len, "%s", s.network.ssid);
  } else {
    snprintf(out, len, "nicht verbunden");
  }
}
void valueIp(const SystemStatus& s, uint32_t, char* out, size_t len) {
  snprintf(out, len, "%s", s.network.wifiConnected ? s.network.ip : (s.network.portalActive ? "192.168.4.1" : "-"));
}
void valueRssi(const SystemStatus& s, uint32_t, char* out, size_t len) {
  if (s.network.wifiConnected) {
    snprintf(out, len, "%d dBm", static_cast<int>(s.network.rssi));
  } else {
    snprintf(out, len, "-");
  }
}
void valueMqtt(const SystemStatus& s, uint32_t, char* out, size_t len) {
  if (!s.network.mqttConfigured) {
    snprintf(out, len, "kein Server");
  } else {
    snprintf(out, len, "%s", s.network.mqttConnected ? "verbunden" : "getrennt");
  }
}
void valuePortal(const SystemStatus& s, uint32_t, char* out, size_t len) {
  snprintf(out, len, "%s", s.network.portalActive ? "läuft" : "OK = Starten");
}

bool whenFlushing(const SystemStatus& s) { return s.controller.mode == Mode::Flushing; }
bool whenFault(const SystemStatus& s) { return s.controller.mode == Mode::Fault; }
bool whenService(const SystemStatus& s) { return s.controller.mode == Mode::Service; }

const Item kStatusItems[] = {
    {"Temp vor Filter", valueTempBefore, Action::None, false, nullptr},
    {"Temp nach Filter", valueTempAfter, Action::None, false, nullptr},
    {"Wasser (Zulauf)", valueWater, Action::None, false, nullptr},
    {"Durchfluss", valueFlow, Action::None, false, nullptr},
    {"Druckschalter", valuePressure, Action::None, false, nullptr},
    {"Ventile", valueValves, Action::None, false, nullptr},
    {"Uhrzeit", valueClock, Action::None, false, nullptr},
};

// Index von "Fehler" in kActionItems, für den Sprung von der Startseite
constexpr uint8_t kFaultItem = 0;

const Item kActionItems[] = {
    {"Fehler", valueFault, Action::AckFault, false, whenFault},
    {"Systemspülung", valueSystemFlush, Action::StartSystem, false, nullptr},
    {"Membranspülung", valueMembraneFlush, Action::StartMembrane, false, nullptr},
    {"Spülung stoppen", valueAbort, Action::AbortFlush, false, whenFlushing},
    {"Service-Modus", valueService, Action::ToggleService, false, nullptr},
    {"Ventil Zulauf", valueFresh, Action::ToggleFresh, false, whenService},
    {"Ventil Abwasser", valueWaste, Action::ToggleWaste, false, whenService},
    {"Ventil Bypass", valueBypass, Action::ToggleBypass, false, whenService},
};

const Item kNetworkItems[] = {
    {"WLAN", valueWifi, Action::None, false, nullptr},
    {"IP-Adresse", valueIp, Action::None, false, nullptr},
    {"Signal", valueRssi, Action::None, false, nullptr},
    {"MQTT", valueMqtt, Action::None, false, nullptr},
    {"WLAN einrichten", valuePortal, Action::WifiPortal, false, nullptr},
};

const Item kSystemItems[] = {
    {"Zähler löschen", valueResetCounter, Action::ResetCounter, true, nullptr},
    {"LED-Test", valueStart, Action::LedTest, false, nullptr},
    {"Werksreset", valueFactoryReset, Action::FactoryReset, true, nullptr},
    {"Version", valueVersion, Action::None, false, nullptr},
};

template <typename T, size_t N>
constexpr uint8_t countOf(const T (&)[N]) {
  return static_cast<uint8_t>(N);
}

constexpr uint8_t kActionsCategory = 1;

const Category kCategories[] = {
    {"Status", kStatusItems, countOf(kStatusItems)},
    {"Aktionen", kActionItems, countOf(kActionItems)},
    {"Netzwerk", kNetworkItems, countOf(kNetworkItems)},
    {"System", kSystemItems, countOf(kSystemItems)},
};
constexpr uint8_t kCategoryCount = countOf(kCategories);

const Item& itemAt(uint8_t category, uint8_t item) { return kCategories[category].items[item]; }

}  // namespace

// ---------------------------------------------------------------------------
// Tasten

void Menu::handleKey(Key key, Press press, const SystemStatus& status, uint32_t nowMs) {
  lastKeyMs_ = nowMs;

  if (key == Key::Left && press == Press::Long) {
    goHome();
    return;
  }
  if (press != Press::Short) {
    return;
  }

  switch (level_) {
    case Level::Home:
      if (key == Key::Ok) {
        if (status.controller.mode == Mode::Fault) {
          openCategory(kActionsCategory, status);
          item_ = kFaultItem;
        } else {
          level_ = Level::Categories;
          category_ = 0;
        }
      }
      break;

    case Level::Categories:
      if (key == Key::Right) {
        category_ = static_cast<uint8_t>((category_ + 1) % kCategoryCount);
      } else if (key == Key::Left) {
        goHome();
      } else {
        openCategory(category_, status);
      }
      break;

    case Level::Items:
      if (key == Key::Right) {
        confirmPending_ = false;
        item_ = static_cast<uint8_t>((item_ + 1) % kCategories[category_].count);
        moveToVisibleItem(status, true);
      } else if (key == Key::Left) {
        confirmPending_ = false;
        level_ = Level::Categories;
      } else {
        activateItem(status, nowMs);
      }
      break;
  }
}

void Menu::goHome() {
  level_ = Level::Home;
  confirmPending_ = false;
  message_ = nullptr;
}

void Menu::openCategory(uint8_t category, const SystemStatus& status) {
  level_ = Level::Items;
  category_ = category;
  item_ = 0;
  confirmPending_ = false;
  moveToVisibleItem(status, true);
}

bool Menu::itemVisible(uint8_t category, uint8_t item, const SystemStatus& status) const {
  const Item& entry = itemAt(category, item);
  return entry.visible == nullptr || entry.visible(status);
}

void Menu::moveToVisibleItem(const SystemStatus& status, bool forward) {
  const uint8_t count = kCategories[category_].count;
  for (uint8_t tries = 0; tries < count; ++tries) {
    if (itemVisible(category_, item_, status)) {
      return;
    }
    item_ = forward ? static_cast<uint8_t>((item_ + 1) % count) : static_cast<uint8_t>((item_ + count - 1) % count);
  }
}

void Menu::activateItem(const SystemStatus& status, uint32_t nowMs) {
  const Item& entry = itemAt(category_, item_);
  if (entry.action == Action::None) {
    return;
  }
  if (entry.confirm && !(confirmPending_ && nowMs - confirmSinceMs_ < kConfirmTimeoutMs)) {
    confirmPending_ = true;
    confirmSinceMs_ = nowMs;
    return;
  }
  confirmPending_ = false;

  const core::ValveMask valves = status.controller.valves;
  const bool service = status.controller.mode == Mode::Service;
  bool ok = true;
  const char* done = "OK";

  switch (entry.action) {
    case Action::None: return;
    case Action::StartSystem:
      ok = host_.sendCommand(Command::startFlush(Program::System));
      done = "Angefordert";
      break;
    case Action::StartMembrane:
      ok = host_.sendCommand(Command::startFlush(Program::Membrane));
      done = "Angefordert";
      break;
    case Action::AbortFlush:
      ok = host_.sendCommand(Command::abortFlush());
      done = "Abgebrochen";
      break;
    case Action::AckFault:
      ok = host_.sendCommand(Command::acknowledgeFault());
      done = "Quittiert";
      break;
    case Action::ToggleService:
      ok = host_.sendCommand(Command::serviceMode(!service));
      done = service ? "Service aus" : "Service an";
      break;
    case Action::ToggleFresh:
      ok = host_.sendCommand(Command::setValve(Valve::FreshWater, !core::isOpen(valves, Valve::FreshWater)));
      break;
    case Action::ToggleWaste:
      ok = host_.sendCommand(Command::setValve(Valve::WasteWater, !core::isOpen(valves, Valve::WasteWater)));
      break;
    case Action::ToggleBypass:
      ok = host_.sendCommand(Command::setValve(Valve::MembraneBypass, !core::isOpen(valves, Valve::MembraneBypass)));
      break;
    case Action::ResetCounter:
      host_.resetWaterCounter();
      done = "Zähler auf 0";
      break;
    case Action::LedTest:
      host_.startLedTest();
      done = "LED-Test läuft";
      break;
    case Action::FactoryReset:
      host_.factoryReset();
      done = "Zurückgesetzt";
      break;
    case Action::WifiPortal:
      host_.startWifiPortal();
      done = "WLAN: RO-Setup";
      break;
  }
  showMessage(ok ? done : "Nicht möglich", nowMs);
}

void Menu::showMessage(const char* text, uint32_t nowMs) {
  message_ = text;
  messageUntilMs_ = nowMs + kMessageMs;
}

// ---------------------------------------------------------------------------
// Zeichnen

void Menu::render(const SystemStatus& status, uint32_t nowMs, Frame& frame) {
  if (level_ != Level::Home && nowMs - lastKeyMs_ >= kIdleTimeoutMs) {
    goHome();
  }
  if (confirmPending_ && nowMs - confirmSinceMs_ >= kConfirmTimeoutMs) {
    confirmPending_ = false;
  }
  if (message_ != nullptr && static_cast<int32_t>(nowMs - messageUntilMs_) >= 0) {
    message_ = nullptr;
  }
  if (level_ == Level::Items) {
    moveToVisibleItem(status, true);  // z. B. "Spülung stoppen" verschwindet nach der Spülung
  }

  frame.clear();
  switch (level_) {
    case Level::Home: renderHome(status, nowMs, frame); break;
    case Level::Categories: renderCategories(frame); break;
    case Level::Items: renderItem(status, nowMs, frame); break;
  }
}

void Menu::renderHome(const SystemStatus& status, uint32_t nowMs, Frame& frame) const {
  const core::ControllerStatus& c = status.controller;
  const core::Measurements& m = status.measurements;
  char text[24];

  frame.print(0, 0, modeLabel(c.mode));
  if (status.clock.valid) {
    snprintf(text, sizeof(text), "%02u:%02u", status.clock.minuteOfDay / 60u, status.clock.minuteOfDay % 60u);
    frame.print(0, 10, text);
  }
  frame.put(0, kCols - 1, wifiIcon(status.network));

  // Warnungen wechseln sich in Zeile 2 mit der normalen Anzeige ab.
  const bool showWarning = c.warnings != 0 && (c.mode == Mode::Idle || c.mode == Mode::Producing) && alternate(nowMs);
  if (showWarning) {
    frame.print(1, 0, (c.warnings & core::kWarnValveLeak) ? "! Ventil undicht" : "! Kein Zulauf");
    return;
  }

  switch (c.mode) {
    case Mode::Idle: {
      char liters[16];
      formatLiters(m.totalLiters, liters, sizeof(liters));
      snprintf(text, sizeof(text), "Zulauf %s", liters);
      frame.print(1, 0, text);
      break;
    }
    case Mode::Producing:
      snprintf(text, sizeof(text), "%.1f L/min", static_cast<double>(m.flowLpm));
      frame.print(1, 0, text);
      formatMinSec(c.inletOpenMs, text, sizeof(text));
      frame.printRight(1, kCols, text);
      break;
    case Mode::Flushing:
      snprintf(text, sizeof(text), "%s %u/%u", programShort(c.program), static_cast<unsigned>(c.step + 1),
               static_cast<unsigned>(c.stepCount));
      frame.print(1, 0, text);
      formatMinSec(c.programRemainingMs, text, sizeof(text));
      frame.printRight(1, kCols, text);
      break;
    case Mode::Service:
      formatValves(c.valves, text, sizeof(text));
      frame.print(1, 0, text);
      break;
    case Mode::Fault:
      frame.print(1, 0, alternate(nowMs) ? "OK = Quittieren" : faultShort(c.fault));
      break;
  }
}

void Menu::renderCategories(Frame& frame) const {
  char text[24];
  snprintf(text, sizeof(text), ">%s", kCategories[category_].title);
  frame.print(0, 0, text);
  snprintf(text, sizeof(text), " %s", kCategories[(category_ + 1) % kCategoryCount].title);
  frame.print(1, 0, text);
}

void Menu::renderItem(const SystemStatus& status, uint32_t nowMs, Frame& frame) const {
  const Item& entry = itemAt(category_, item_);
  frame.print(0, 0, entry.title);

  if (message_ != nullptr) {
    frame.print(1, 0, message_);
  } else if (confirmPending_) {
    frame.print(1, 0, "OK = Bestätigen");
  } else if (entry.value != nullptr) {
    char text[32];
    entry.value(status, nowMs, text, sizeof(text));
    frame.print(1, 0, text);
  }
}

}  // namespace ui
