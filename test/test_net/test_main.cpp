// Unit-Tests für das Auswerten von MQTT-Nutzlasten und die Texte für Home Assistant.
// Laufen auf dem Mac: pio test -e native

#include <string.h>
#include <unity.h>

#include "net/protocol.h"

using config::FlushTime;

void setUp() {}
void tearDown() {}

void test_copy_payload_trims_and_terminates() {
  const char raw[] = "  08:30 \n";
  char out[16];
  net::copyPayload(reinterpret_cast<const uint8_t*>(raw), strlen(raw), out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("08:30", out);

  char tiny[4];
  net::copyPayload(reinterpret_cast<const uint8_t*>("PRESS"), 5, tiny, sizeof(tiny));
  TEST_ASSERT_EQUAL_STRING("PRE", tiny);
}

void test_parse_on_off() {
  bool on = false;
  TEST_ASSERT_TRUE(net::parseOnOff("ON", on));
  TEST_ASSERT_TRUE(on);
  TEST_ASSERT_TRUE(net::parseOnOff("off", on));
  TEST_ASSERT_FALSE(on);
  TEST_ASSERT_TRUE(net::parseOnOff("1", on));
  TEST_ASSERT_TRUE(on);
  TEST_ASSERT_FALSE(net::parseOnOff("vielleicht", on));
  TEST_ASSERT_TRUE(net::isPress("PRESS"));
  TEST_ASSERT_FALSE(net::isPress("OFF"));
}

void test_parse_hours() {
  uint8_t h = 99;
  TEST_ASSERT_TRUE(net::parseHours("8", 168, h));
  TEST_ASSERT_EQUAL_UINT8(8, h);
  TEST_ASSERT_TRUE(net::parseHours("24.0", 168, h));
  TEST_ASSERT_EQUAL_UINT8(24, h);
  TEST_ASSERT_TRUE(net::parseHours("0", 168, h));
  TEST_ASSERT_EQUAL_UINT8(0, h);
  TEST_ASSERT_FALSE(net::parseHours("169", 168, h));
  TEST_ASSERT_FALSE(net::parseHours("-1", 168, h));
  TEST_ASSERT_FALSE(net::parseHours("2.5", 168, h));
  TEST_ASSERT_FALSE(net::parseHours("abc", 168, h));
  TEST_ASSERT_FALSE(net::parseHours("", 168, h));
}

void test_parse_flush_time() {
  FlushTime t;
  TEST_ASSERT_TRUE(net::parseFlushTime("03:00", t));
  TEST_ASSERT_TRUE(t.enabled);
  TEST_ASSERT_EQUAL_UINT8(3, t.hour);
  TEST_ASSERT_EQUAL_UINT8(0, t.minute);

  TEST_ASSERT_TRUE(net::parseFlushTime("0:00", t));  // Mitternacht ist gültig
  TEST_ASSERT_TRUE(t.enabled);
  TEST_ASSERT_EQUAL_UINT8(0, t.hour);

  TEST_ASSERT_TRUE(net::parseFlushTime("23:59:30", t));
  TEST_ASSERT_EQUAL_UINT8(23, t.hour);
  TEST_ASSERT_EQUAL_UINT8(59, t.minute);

  TEST_ASSERT_TRUE(net::parseFlushTime("--:--", t));
  TEST_ASSERT_FALSE(t.enabled);
  TEST_ASSERT_EQUAL_UINT8(23, t.hour);  // Uhrzeit bleibt erhalten
  TEST_ASSERT_TRUE(net::parseFlushTime("", t));
  TEST_ASSERT_TRUE(net::parseFlushTime("aus", t));

  FlushTime unchanged;
  unchanged.enabled = true;
  unchanged.hour = 5;
  TEST_ASSERT_FALSE(net::parseFlushTime("24:00", unchanged));
  TEST_ASSERT_FALSE(net::parseFlushTime("12:60", unchanged));
  TEST_ASSERT_FALSE(net::parseFlushTime("12:5", unchanged));
  TEST_ASSERT_FALSE(net::parseFlushTime("mittags", unchanged));
  TEST_ASSERT_TRUE(unchanged.enabled);
  TEST_ASSERT_EQUAL_UINT8(5, unchanged.hour);
}

void test_format_flush_time() {
  FlushTime t;
  char out[8];
  net::formatFlushTime(t, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("--:--", out);
  t.enabled = true;
  t.hour = 3;
  t.minute = 5;
  net::formatFlushTime(t, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("03:05", out);
}

void test_state_and_problem_texts() {
  core::ControllerStatus c;
  char out[64];

  net::stateText(c, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Bereit", out);
  TEST_ASSERT_FALSE(net::hasProblem(c));
  net::problemText(c, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("OK", out);

  c.mode = core::Mode::Flushing;
  c.program = core::Program::Membrane;
  net::stateText(c, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Spült: Membranspülung", out);

  c.mode = core::Mode::Idle;
  c.warnings = core::kWarnNoInletFlow;
  TEST_ASSERT_TRUE(net::hasProblem(c));
  net::problemText(c, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Warnung: Zulauf offen, aber kein Durchfluss", out);

  c.mode = core::Mode::Fault;
  c.fault = core::FaultReason::InletOpenTooLong;
  net::problemText(c, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Fehler: Zulauf zu lange offen", out);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_copy_payload_trims_and_terminates);
  RUN_TEST(test_parse_on_off);
  RUN_TEST(test_parse_hours);
  RUN_TEST(test_parse_flush_time);
  RUN_TEST(test_format_flush_time);
  RUN_TEST(test_state_and_problem_texts);
  return UNITY_END();
}
