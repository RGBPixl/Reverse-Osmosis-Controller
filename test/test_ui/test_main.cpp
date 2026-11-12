// Unit-Tests für Menü, Startseite, LCD-Zeichensatz und Datumsrechnung.
// Laufen auf dem Mac: pio test -e native

#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "core/status.h"
#include "ui/brightness.h"
#include "ui/frame.h"
#include "ui/menu.h"
#include "util/clock.h"

using namespace core;
using ui::Frame;
using ui::Key;
using ui::Menu;
using ui::Press;

namespace {

struct FakeHost : ui::MenuHost {
  int commands = 0;
  Command last = Command::abortFlush();
  bool accept = true;
  int counterResets = 0;
  int factoryResets = 0;
  int ledTests = 0;
  int portals = 0;

  bool sendCommand(const Command& command) override {
    ++commands;
    last = command;
    return accept;
  }
  void resetWaterCounter() override { ++counterResets; }
  void factoryReset() override { ++factoryResets; }
  void startLedTest() override { ++ledTests; }
  void startWifiPortal() override { ++portals; }
};

// Zeile ohne Leerzeichen am Ende
const char* row(const Frame& frame, uint8_t r) {
  static char text[ui::kCols + 1];
  frame.rowText(r, text);
  for (int i = ui::kCols - 1; i >= 0 && text[i] == ' '; --i) {
    text[i] = '\0';
  }
  return text;
}

struct MenuSim {
  FakeHost host;
  Menu menu{host};
  SystemStatus status;
  Frame frame;
  uint32_t now = 1000;

  void press(Key key, Press p = Press::Short) {
    menu.handleKey(key, p, status, now);
    draw();
  }
  void draw() { menu.render(status, now, frame); }
  void wait(uint32_t ms) {
    now += ms;
    draw();
  }
};

}  // namespace

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------------------
// LCD-Zeichensatz

void test_frame_converts_umlauts() {
  Frame f;
  f.setLine(0, "Spülung äöß°");
  TEST_ASSERT_EQUAL_STRING("Sp\xF5lung \xE1\xEF\xE2\xDF", row(f, 0));
  f.setLine(1, "ÄÖÜ €");
  TEST_ASSERT_EQUAL_STRING("\xE1\xEF\xF5 ?", row(f, 1));
}

void test_frame_cuts_long_text_and_aligns_right() {
  Frame f;
  f.setLine(0, "12345678901234567890");
  TEST_ASSERT_EQUAL_STRING("1234567890123456", row(f, 0));
  f.printRight(1, ui::kCols, "2:15");
  TEST_ASSERT_EQUAL_STRING("            2:15", row(f, 1));
  TEST_ASSERT_EQUAL_UINT32(5, ui::lcdLength("Spül°"));
}

// ---------------------------------------------------------------------------
// Helligkeit

void test_perceived_brightness_to_duty() {
  TEST_ASSERT_EQUAL_UINT8(0, ui::perceivedToDuty(0));
  TEST_ASSERT_EQUAL_UINT8(1, ui::perceivedToDuty(1));  // bleibt sichtbar
  TEST_ASSERT_EQUAL_UINT8(12, ui::perceivedToDuty(25));
  TEST_ASSERT_EQUAL_UINT8(55, ui::perceivedToDuty(50));  // 255 * 0,5^2,2 = 55,49
  TEST_ASSERT_EQUAL_UINT8(255, ui::perceivedToDuty(100));
  TEST_ASSERT_EQUAL_UINT8(255, ui::perceivedToDuty(200));
}

// ---------------------------------------------------------------------------
// Datumsrechnung

void test_days_from_civil_and_back() {
  TEST_ASSERT_EQUAL_INT32(0, util::daysFromCivil(1970, 1, 1));
  TEST_ASSERT_EQUAL_INT32(20404, util::daysFromCivil(2025, 11, 12));

  const int32_t leap = util::daysFromCivil(2024, 2, 29);
  TEST_ASSERT_EQUAL_INT32(leap + 1, util::daysFromCivil(2024, 3, 1));
  TEST_ASSERT_EQUAL_INT32(util::daysFromCivil(2025, 12, 31) + 1, util::daysFromCivil(2026, 1, 1));

  const util::CivilDate d = util::civilFromDays(leap);
  TEST_ASSERT_EQUAL_INT32(2024, d.year);
  TEST_ASSERT_EQUAL_UINT8(2, d.month);
  TEST_ASSERT_EQUAL_UINT8(29, d.day);
}

// ---------------------------------------------------------------------------
// Startseite

void test_home_idle_shows_time_and_counter() {
  MenuSim sim;
  sim.status.measurements.totalLiters = 1234.56;
  sim.status.clock.valid = true;
  sim.status.clock.minuteOfDay = 14 * 60 + 3;
  sim.draw();
  // Spalte 15: WLAN-Symbol, hier "kein WLAN" (Code 5)
  TEST_ASSERT_EQUAL_STRING("Bereit    14:03\x05", row(sim.frame, 0));
  TEST_ASSERT_EQUAL_STRING("Zulauf 1234.6 L", row(sim.frame, 1));
}

void test_home_wifi_icon_shows_signal_and_portal() {
  MenuSim sim;
  sim.status.network.wifiConnected = true;
  sim.status.network.rssi = -65;
  sim.draw();
  TEST_ASSERT_EQUAL_INT(ui::kCharWifi2, sim.frame.cells[0][15]);
  sim.status.network.rssi = -50;
  sim.draw();
  TEST_ASSERT_EQUAL_INT(ui::kCharWifi3, sim.frame.cells[0][15]);
  sim.status.network.portalActive = true;
  sim.draw();
  TEST_ASSERT_EQUAL_INT('P', sim.frame.cells[0][15]);
}

void test_home_flushing_shows_program_and_remaining_time() {
  MenuSim sim;
  ControllerStatus& c = sim.status.controller;
  c.mode = Mode::Flushing;
  c.program = Program::System;
  c.step = 1;
  c.stepCount = 4;
  c.programRemainingMs = 135000;
  sim.draw();
  TEST_ASSERT_EQUAL_STRING("Sp\xF5lung        \x05", row(sim.frame, 0));
  TEST_ASSERT_EQUAL_STRING("System 2/4  2:15", row(sim.frame, 1));
}

void test_home_warning_alternates_with_counter() {
  MenuSim sim;
  sim.status.controller.warnings = kWarnValveLeak;
  sim.now = 0;
  sim.draw();
  TEST_ASSERT_EQUAL_STRING("Zulauf 0.0 L", row(sim.frame, 1));
  sim.wait(Menu::kAlternateMs);
  TEST_ASSERT_EQUAL_STRING("! Ventil undicht", row(sim.frame, 1));
}

// ---------------------------------------------------------------------------
// Menü

void test_menu_navigation_between_categories() {
  MenuSim sim;
  sim.press(Key::Ok);
  TEST_ASSERT_EQUAL_STRING(">Status", row(sim.frame, 0));
  TEST_ASSERT_EQUAL_STRING(" Aktionen", row(sim.frame, 1));
  sim.press(Key::Right);
  TEST_ASSERT_EQUAL_STRING(">Aktionen", row(sim.frame, 0));
  sim.press(Key::Left);
  TEST_ASSERT_TRUE(sim.menu.atHome());
}

void test_start_system_flush_from_menu() {
  MenuSim sim;
  sim.press(Key::Ok);
  sim.press(Key::Right);
  sim.press(Key::Ok);  // Aktionen öffnen: "Fehler" ist ausgeblendet
  TEST_ASSERT_EQUAL_STRING("Systemsp\xF5lung", row(sim.frame, 0));
  TEST_ASSERT_EQUAL_STRING("OK = Starten", row(sim.frame, 1));

  sim.press(Key::Ok);
  TEST_ASSERT_EQUAL_INT(1, sim.host.commands);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::StartFlush), static_cast<int>(sim.host.last.type));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Program::System), static_cast<int>(sim.host.last.program));
  TEST_ASSERT_EQUAL_STRING("Angefordert", row(sim.frame, 1));

  sim.wait(Menu::kMessageMs);
  TEST_ASSERT_EQUAL_STRING("OK = Starten", row(sim.frame, 1));
}

void test_rejected_command_shows_message() {
  MenuSim sim;
  sim.host.accept = false;
  sim.press(Key::Ok);
  sim.press(Key::Right);
  sim.press(Key::Ok);
  sim.press(Key::Ok);
  TEST_ASSERT_EQUAL_STRING("Nicht m\xEFglich", row(sim.frame, 1));
}

void test_network_category() {
  MenuSim sim;
  sim.press(Key::Ok);
  sim.press(Key::Right);
  sim.press(Key::Right);
  TEST_ASSERT_EQUAL_STRING(">Netzwerk", row(sim.frame, 0));
  sim.press(Key::Ok);
  TEST_ASSERT_EQUAL_STRING("WLAN", row(sim.frame, 0));
  TEST_ASSERT_EQUAL_STRING("nicht verbunden", row(sim.frame, 1));

  sim.status.network.wifiConnected = true;
  snprintf(sim.status.network.ssid, sizeof(sim.status.network.ssid), "Zuhause");
  snprintf(sim.status.network.ip, sizeof(sim.status.network.ip), "192.168.1.50");
  sim.draw();
  TEST_ASSERT_EQUAL_STRING("Zuhause", row(sim.frame, 1));
  sim.press(Key::Right);
  TEST_ASSERT_EQUAL_STRING("192.168.1.50", row(sim.frame, 1));
  sim.press(Key::Right);
  sim.press(Key::Right);
  TEST_ASSERT_EQUAL_STRING("MQTT", row(sim.frame, 0));
  TEST_ASSERT_EQUAL_STRING("kein Server", row(sim.frame, 1));

  sim.press(Key::Right);
  TEST_ASSERT_EQUAL_STRING("WLAN einrichten", row(sim.frame, 0));
  sim.press(Key::Ok);
  TEST_ASSERT_EQUAL_INT(1, sim.host.portals);
  TEST_ASSERT_EQUAL_STRING("WLAN: RO-Setup", row(sim.frame, 1));
}

void test_counter_reset_needs_confirmation() {
  MenuSim sim;
  sim.press(Key::Ok);
  sim.press(Key::Right);
  sim.press(Key::Right);
  sim.press(Key::Right);
  sim.press(Key::Ok);  // System
  TEST_ASSERT_EQUAL_STRING("Z\xE1hler l\xEFschen", row(sim.frame, 0));

  sim.press(Key::Ok);
  TEST_ASSERT_EQUAL_INT(0, sim.host.counterResets);
  TEST_ASSERT_EQUAL_STRING("OK = Best\xE1tigen", row(sim.frame, 1));

  sim.wait(Menu::kConfirmTimeoutMs);  // Bestätigung verfällt
  TEST_ASSERT_EQUAL_STRING("OK = auf 0", row(sim.frame, 1));
  sim.press(Key::Ok);
  TEST_ASSERT_EQUAL_INT(0, sim.host.counterResets);

  sim.press(Key::Ok);
  TEST_ASSERT_EQUAL_INT(1, sim.host.counterResets);
  TEST_ASSERT_EQUAL_STRING("Z\xE1hler auf 0", row(sim.frame, 1));
}

void test_valve_items_only_in_service_mode() {
  MenuSim sim;
  sim.press(Key::Ok);
  sim.press(Key::Right);
  sim.press(Key::Ok);  // Aktionen
  sim.press(Key::Right);
  sim.press(Key::Right);
  TEST_ASSERT_EQUAL_STRING("Service-Modus", row(sim.frame, 0));
  sim.press(Key::Right);  // Ventile sind ausgeblendet: zurück zum Anfang
  TEST_ASSERT_EQUAL_STRING("Systemsp\xF5lung", row(sim.frame, 0));

  sim.status.controller.mode = Mode::Service;
  sim.press(Key::Right);
  sim.press(Key::Right);
  sim.press(Key::Right);
  TEST_ASSERT_EQUAL_STRING("Ventil Zulauf", row(sim.frame, 0));
  TEST_ASSERT_EQUAL_STRING("zu (OK = auf)", row(sim.frame, 1));
  sim.press(Key::Ok);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::SetValve), static_cast<int>(sim.host.last.type));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Valve::FreshWater), static_cast<int>(sim.host.last.valve));
  TEST_ASSERT_TRUE(sim.host.last.on);
}

void test_fault_shortcut_from_home() {
  MenuSim sim;
  sim.status.controller.mode = Mode::Fault;
  sim.status.controller.fault = FaultReason::InletOpenTooLong;
  sim.now = 0;
  sim.draw();
  TEST_ASSERT_EQUAL_STRING("FEHLER         \x05", row(sim.frame, 0));
  TEST_ASSERT_EQUAL_STRING("Zulauf zu lange", row(sim.frame, 1));

  sim.press(Key::Ok);
  TEST_ASSERT_EQUAL_STRING("Fehler", row(sim.frame, 0));
  sim.press(Key::Ok);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandType::AcknowledgeFault), static_cast<int>(sim.host.last.type));
}

void test_menu_returns_home_after_timeout() {
  MenuSim sim;
  sim.press(Key::Ok);
  sim.wait(Menu::kIdleTimeoutMs - 1);
  TEST_ASSERT_FALSE(sim.menu.atHome());
  sim.wait(1);
  TEST_ASSERT_TRUE(sim.menu.atHome());
}

void test_long_left_goes_home() {
  MenuSim sim;
  sim.press(Key::Ok);
  sim.press(Key::Ok);
  TEST_ASSERT_FALSE(sim.menu.atHome());
  sim.press(Key::Left, Press::Long);
  TEST_ASSERT_TRUE(sim.menu.atHome());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_frame_converts_umlauts);
  RUN_TEST(test_frame_cuts_long_text_and_aligns_right);
  RUN_TEST(test_perceived_brightness_to_duty);
  RUN_TEST(test_days_from_civil_and_back);
  RUN_TEST(test_home_idle_shows_time_and_counter);
  RUN_TEST(test_home_wifi_icon_shows_signal_and_portal);
  RUN_TEST(test_home_flushing_shows_program_and_remaining_time);
  RUN_TEST(test_home_warning_alternates_with_counter);
  RUN_TEST(test_menu_navigation_between_categories);
  RUN_TEST(test_start_system_flush_from_menu);
  RUN_TEST(test_rejected_command_shows_message);
  RUN_TEST(test_network_category);
  RUN_TEST(test_counter_reset_needs_confirmation);
  RUN_TEST(test_valve_items_only_in_service_mode);
  RUN_TEST(test_fault_shortcut_from_home);
  RUN_TEST(test_menu_returns_home_after_timeout);
  RUN_TEST(test_long_left_goes_home);
  return UNITY_END();
}
