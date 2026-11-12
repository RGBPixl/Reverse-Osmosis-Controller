// Unit-Tests der Steuerlogik. Laufen auf dem Mac: pio test -e native
//
// Die Simulation (Sim) ruft tick() alle 100 ms auf, wie später die Firmware,
// und lässt Druck, Durchfluss und Uhrzeit von außen vorgeben.

#include <math.h>
#include <unity.h>

#include "config/settings.h"
#include "core/controller.h"
#include "core/scheduler.h"

using namespace core;
using config::Settings;

namespace {

constexpr uint32_t kDay0 = 20404;                              // 12.11.2025
constexpr uint32_t kEpoch0 = kDay0 * 86400UL + 10 * 3600UL;  // 10:00:00

const ValveMask kFresh = valveBit(Valve::FreshWater);
const ValveMask kWaste = valveBit(Valve::WasteWater);
const ValveMask kBypass = valveBit(Valve::MembraneBypass);

WallClock clockAt(uint32_t epoch) {
  WallClock c;
  c.valid = true;
  c.epoch = epoch;
  c.day = epoch / 86400UL;
  c.minuteOfDay = static_cast<uint16_t>((epoch % 86400UL) / 60);
  return c;
}

WallClock clockAtTime(uint32_t day, uint32_t hour, uint32_t minute) {
  return clockAt(day * 86400UL + hour * 3600UL + minute * 60UL);
}

struct Sim {
  Controller ctrl;
  TickInput in;
  TickOutput out;
  bool clockValid = true;
  uint32_t startMs = 1000;
  bool sawSaveCounter = false;
  uint32_t savedBootEpoch = 0;

  void begin(const Settings& settings, uint32_t lastBootEpoch) {
    in.nowMs = startMs;
    in.pressurized = true;
    in.flowLpm = 0.0f;
    in.msSinceFlowPulse = 0;
    ctrl.begin(settings, in.nowMs, lastBootEpoch);
    tickOnce();
  }

  // Letzter Start vor 1 min: keine Startspülung, damit Tests bei "Bereit" beginnen.
  void beginRecent(const Settings& settings = Settings()) { begin(settings, kEpoch0 - 60); }

  void tickOnce() {
    in.clock = clockValid ? clockAt(kEpoch0 + (in.nowMs - startMs) / 1000) : WallClock();
    out = ctrl.tick(in);
    if (out.saveCounter) sawSaveCounter = true;
    if (out.saveBootEpoch != 0) savedBootEpoch = out.saveBootEpoch;
  }

  void run(uint32_t ms, uint32_t stepMs = 100) {
    const uint32_t end = in.nowMs + ms;
    while (in.nowMs < end) {
      in.nowMs += stepMs;
      in.msSinceFlowPulse = in.flowLpm > 0.0f ? 0 : in.msSinceFlowPulse + stepMs;
      tickOnce();
    }
  }

  ControllerStatus status() const { return ctrl.status(); }
  Mode mode() const { return ctrl.status().mode; }
};

#define ASSERT_MODE(expected, sim) \
  TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(expected), static_cast<int>((sim).mode()), "Modus")
#define ASSERT_VALVES(expected, sim) TEST_ASSERT_EQUAL_HEX8_MESSAGE((expected), (sim).out.valves, "Ventile")
#define ASSERT_PROGRAM(expected, sim) \
  TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(expected), static_cast<int>((sim).status().program), "Programm")
#define ASSERT_FAULT(expected, sim) \
  TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(expected), static_cast<int>((sim).status().fault), "Fehler")

}  // namespace

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------------------
// Einstellungen

void test_settings_sanitize_clamps_invalid_values() {
  Settings s;
  s.flushTimes[0].hour = 30;
  s.flushTimes[1].minute = 75;
  s.litersPerPulse = NAN;
  s.mqttPort = 0;
  s.membraneFlushS = 1;
  s.mqttServer[sizeof(s.mqttServer) - 1] = 'x';
  s.statusLedPercent = 150;

  TEST_ASSERT_TRUE(config::sanitize(s));
  TEST_ASSERT_EQUAL_UINT8(23, s.flushTimes[0].hour);
  TEST_ASSERT_EQUAL_UINT8(59, s.flushTimes[1].minute);
  TEST_ASSERT_EQUAL_FLOAT(0.00052f, s.litersPerPulse);
  TEST_ASSERT_EQUAL_UINT16(1883, s.mqttPort);
  TEST_ASSERT_EQUAL_UINT16(config::kMinMembraneFlushS, s.membraneFlushS);
  TEST_ASSERT_EQUAL_INT('\0', s.mqttServer[sizeof(s.mqttServer) - 1]);
  TEST_ASSERT_EQUAL_UINT8(100, s.statusLedPercent);

  Settings valid;
  TEST_ASSERT_FALSE(config::sanitize(valid));
}

// ---------------------------------------------------------------------------
// Zeitplan

void test_scheduler_interval_due_until_finished() {
  Settings s;
  s.systemFlushIntervalH = 8;
  Scheduler sch;
  sch.begin(0);
  const uint32_t h = 3600000UL;
  const WallClock noClock;

  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, noClock, 8 * h - 1));
  TEST_ASSERT_EQUAL_UINT8(programBit(Program::System), sch.tick(s, noClock, 8 * h));
  TEST_ASSERT_EQUAL_UINT8(programBit(Program::System), sch.tick(s, noClock, 8 * h + 5000));
  sch.programFinished(Program::System, 8 * h + 5000);
  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, noClock, 8 * h + 6000));
  TEST_ASSERT_EQUAL_UINT8(programBit(Program::System), sch.tick(s, noClock, 16 * h + 5000));
}

void test_scheduler_membrane_interval() {
  Settings s;
  s.membraneFlushIntervalH = 24;
  Scheduler sch;
  sch.begin(0);
  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, WallClock(), 24 * 3600000UL - 1));
  TEST_ASSERT_EQUAL_UINT8(programBit(Program::Membrane), sch.tick(s, WallClock(), 24 * 3600000UL));
}

void test_scheduler_interval_zero_is_off() {
  Settings s;
  Scheduler sch;
  sch.begin(0);
  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, WallClock(), 1000UL * 3600000UL));
}

void test_scheduler_fixed_time_once_per_day() {
  Settings s;
  s.flushTimes[0].enabled = true;
  s.flushTimes[0].hour = 3;
  s.flushTimes[0].minute = 0;
  Scheduler sch;
  sch.begin(0);

  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, clockAtTime(kDay0, 2, 59), 0));
  TEST_ASSERT_EQUAL_UINT8(programBit(Program::System), sch.tick(s, clockAtTime(kDay0, 3, 0), 0));
  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, clockAtTime(kDay0, 3, 0), 0));
  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, clockAtTime(kDay0, 3, 1), 0));
  TEST_ASSERT_EQUAL_UINT8(programBit(Program::System), sch.tick(s, clockAtTime(kDay0 + 1, 3, 0), 0));
}

void test_scheduler_fixed_time_needs_valid_clock() {
  Settings s;
  s.flushTimes[0].enabled = true;
  s.flushTimes[0].hour = 3;
  Scheduler sch;
  sch.begin(0);
  WallClock invalid = clockAtTime(kDay0, 3, 0);
  invalid.valid = false;
  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, invalid, 0));
}

void test_scheduler_fixed_time_disabled() {
  Settings s;
  s.flushTimes[0].enabled = false;
  s.flushTimes[0].hour = 3;
  Scheduler sch;
  sch.begin(0);
  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, clockAtTime(kDay0, 3, 0), 0));
}

void test_scheduler_midnight_is_a_valid_time() {
  Settings s;
  s.flushTimes[0].enabled = true;  // 00:00
  Scheduler sch;
  sch.begin(0);
  TEST_ASSERT_EQUAL_UINT8(programBit(Program::System), sch.tick(s, clockAtTime(kDay0, 0, 0), 0));
  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, clockAtTime(kDay0, 0, 1), 0));
}

void test_scheduler_late_evening_time_not_repeated_after_midnight() {
  Settings s;
  s.flushTimes[0].enabled = true;
  s.flushTimes[0].hour = 23;
  s.flushTimes[0].minute = 59;
  Scheduler sch;
  sch.begin(0);
  TEST_ASSERT_EQUAL_UINT8(programBit(Program::System), sch.tick(s, clockAtTime(kDay0, 23, 59), 0));
  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, clockAtTime(kDay0 + 1, 0, 0), 0));
}

void test_scheduler_catches_up_after_clock_jump() {
  Settings s;
  s.flushTimes[0].enabled = true;
  s.flushTimes[0].hour = 3;
  Scheduler sch;
  sch.begin(0);
  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, clockAtTime(kDay0, 2, 58), 0));
  TEST_ASSERT_EQUAL_UINT8(programBit(Program::System), sch.tick(s, clockAtTime(kDay0, 3, 1), 0));
  TEST_ASSERT_EQUAL_UINT8(0, sch.tick(s, clockAtTime(kDay0, 3, 2), 0));
}

void test_scheduler_changed_time_runs_again_same_day() {
  Settings s;
  s.flushTimes[0].enabled = true;
  s.flushTimes[0].hour = 3;
  Scheduler sch;
  sch.begin(0);
  TEST_ASSERT_EQUAL_UINT8(programBit(Program::System), sch.tick(s, clockAtTime(kDay0, 3, 0), 0));
  s.flushTimes[0].hour = 15;
  TEST_ASSERT_EQUAL_UINT8(programBit(Program::System), sch.tick(s, clockAtTime(kDay0, 15, 0), 0));
}

// ---------------------------------------------------------------------------
// Start und Startspülung

void test_starts_idle_with_valves_closed() {
  Sim sim;
  sim.beginRecent();
  ASSERT_MODE(Mode::Idle, sim);
  ASSERT_VALVES(kAllClosed, sim);
  TEST_ASSERT_EQUAL_UINT32(kEpoch0, sim.savedBootEpoch);
}

void test_startup_flush_skipped_after_recent_boot() {
  Sim sim;
  sim.begin(Settings(), kEpoch0 - 1800);
  sim.run(70000, 500);
  ASSERT_MODE(Mode::Idle, sim);
  TEST_ASSERT_FALSE(sim.sawSaveCounter);
}

void test_startup_flush_runs_after_old_boot() {
  Sim sim;
  sim.begin(Settings(), kEpoch0 - 7200);
  ASSERT_MODE(Mode::Flushing, sim);
  ASSERT_PROGRAM(Program::Startup, sim);
  ASSERT_VALVES(kFresh, sim);

  sim.run(5000);
  ASSERT_VALVES(kFresh | kBypass, sim);
  sim.run(10000);
  ASSERT_VALVES(kFresh | kWaste, sim);
  sim.run(10000);
  ASSERT_VALVES(kFresh | kBypass, sim);
  sim.run(10000);
  ASSERT_MODE(Mode::Idle, sim);
  ASSERT_VALVES(kAllClosed, sim);
  TEST_ASSERT_TRUE(sim.sawSaveCounter);
}

void test_startup_flush_runs_without_boot_record() {
  Sim sim;
  sim.begin(Settings(), 0);
  ASSERT_MODE(Mode::Flushing, sim);
  ASSERT_PROGRAM(Program::Startup, sim);
}

void test_startup_flush_runs_offline_after_waiting_for_clock() {
  Sim sim;
  sim.clockValid = false;
  sim.begin(Settings(), kEpoch0 - 60);
  sim.run(59000, 500);
  ASSERT_MODE(Mode::Idle, sim);
  TEST_ASSERT_TRUE(sim.status().startupDecisionPending);

  sim.run(1000, 500);
  ASSERT_MODE(Mode::Flushing, sim);
  ASSERT_PROGRAM(Program::Startup, sim);
  TEST_ASSERT_EQUAL_UINT32(0, sim.savedBootEpoch);
}

// ---------------------------------------------------------------------------
// Druckregelung

void test_pressure_control_respects_hold_time() {
  Sim sim;
  sim.beginRecent();
  sim.in.pressurized = false;
  sim.run(5000);  // erst 10 s nach dem Start darf der Zulauf öffnen
  ASSERT_MODE(Mode::Idle, sim);
  sim.run(5000);
  ASSERT_MODE(Mode::Producing, sim);
  ASSERT_VALVES(kFresh, sim);

  sim.in.pressurized = true;
  sim.run(5000);  // Zulauf mindestens 10 s offen
  ASSERT_MODE(Mode::Producing, sim);
  sim.run(5000);
  ASSERT_MODE(Mode::Idle, sim);
  ASSERT_VALVES(kAllClosed, sim);
}

// ---------------------------------------------------------------------------
// Spülprogramme

void test_system_flush_sequence() {
  Sim sim;
  sim.beginRecent();
  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::startFlush(Program::System)));
  sim.run(100);
  ASSERT_MODE(Mode::Flushing, sim);
  ASSERT_PROGRAM(Program::System, sim);
  ASSERT_VALVES(kFresh, sim);
  TEST_ASSERT_EQUAL_UINT32(181000, sim.status().programRemainingMs);

  sim.run(1000);
  ASSERT_VALVES(kFresh | kBypass, sim);
  sim.run(60000);
  ASSERT_VALVES(kFresh | kWaste, sim);
  sim.run(60000);
  ASSERT_VALVES(kFresh | kBypass, sim);
  sim.run(59900);
  ASSERT_MODE(Mode::Flushing, sim);
  sim.run(100);
  ASSERT_MODE(Mode::Idle, sim);
  ASSERT_VALVES(kAllClosed, sim);
  TEST_ASSERT_TRUE(sim.sawSaveCounter);
}

void test_membrane_flush_takes_five_minutes() {
  Sim sim;
  sim.beginRecent();
  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::startFlush(Program::Membrane)));
  sim.run(100);
  ASSERT_VALVES(kFresh | kWaste | kBypass, sim);
  sim.run(299900);
  ASSERT_MODE(Mode::Flushing, sim);
  sim.run(100);
  ASSERT_MODE(Mode::Idle, sim);
}

void test_flush_interrupts_production() {
  Sim sim;
  sim.beginRecent();
  sim.in.pressurized = false;
  sim.run(10000);
  ASSERT_MODE(Mode::Producing, sim);

  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::startFlush(Program::System)));
  sim.run(100);
  ASSERT_MODE(Mode::Flushing, sim);
  sim.run(181000);
  ASSERT_MODE(Mode::Idle, sim);
  sim.run(10000);  // Druck fehlt weiterhin: nach der Haltezeit wieder Produktion
  ASSERT_MODE(Mode::Producing, sim);
}

void test_request_during_flush_is_queued_and_duplicate_rejected() {
  Sim sim;
  sim.beginRecent();
  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::startFlush(Program::System)));
  sim.run(100);
  TEST_ASSERT_FALSE(sim.ctrl.handle(Command::startFlush(Program::System)));
  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::startFlush(Program::Membrane)));
  TEST_ASSERT_TRUE(sim.status().pending[static_cast<uint8_t>(Program::Membrane)]);

  sim.run(181000);
  ASSERT_MODE(Mode::Idle, sim);
  sim.run(100);
  ASSERT_MODE(Mode::Flushing, sim);
  ASSERT_PROGRAM(Program::Membrane, sim);
}

void test_abort_flush() {
  Sim sim;
  sim.beginRecent();
  TEST_ASSERT_FALSE(sim.ctrl.handle(Command::abortFlush()));
  sim.ctrl.handle(Command::startFlush(Program::System));
  sim.run(5000);
  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::abortFlush()));
  sim.run(100);
  ASSERT_MODE(Mode::Idle, sim);
  ASSERT_VALVES(kAllClosed, sim);
}

void test_fixed_flush_time_starts_system_flush() {
  Settings s;
  s.flushTimes[0].enabled = true;
  s.flushTimes[0].hour = 10;
  s.flushTimes[0].minute = 1;
  Sim sim;
  sim.beginRecent(s);  // Uhr steht auf 10:00:00
  sim.run(59000);
  ASSERT_MODE(Mode::Idle, sim);
  sim.run(1000);
  ASSERT_MODE(Mode::Flushing, sim);
  ASSERT_PROGRAM(Program::System, sim);
}

// ---------------------------------------------------------------------------
// Service-Modus

void test_service_mode_manual_valves_and_queue() {
  Sim sim;
  sim.beginRecent();
  TEST_ASSERT_FALSE(sim.ctrl.handle(Command::setValve(Valve::WasteWater, true)));
  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::serviceMode(true)));
  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::setValve(Valve::WasteWater, true)));
  sim.run(100);
  ASSERT_MODE(Mode::Service, sim);
  ASSERT_VALVES(kWaste, sim);

  sim.in.pressurized = false;  // Automatik pausiert: keine Produktion
  sim.run(20000);
  ASSERT_MODE(Mode::Service, sim);
  ASSERT_VALVES(kWaste, sim);

  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::startFlush(Program::System)));
  sim.run(100);
  ASSERT_MODE(Mode::Service, sim);

  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::serviceMode(false)));
  sim.run(100);
  ASSERT_MODE(Mode::Flushing, sim);  // vorgemerkte Spülung wird nachgeholt
  ASSERT_PROGRAM(Program::System, sim);
}

void test_entering_service_aborts_flush() {
  Sim sim;
  sim.beginRecent();
  sim.ctrl.handle(Command::startFlush(Program::System));
  sim.run(100);
  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::serviceMode(true)));
  sim.run(100);
  ASSERT_MODE(Mode::Service, sim);
  ASSERT_VALVES(kAllClosed, sim);
}

void test_service_mode_inlet_limit() {
  Sim sim;
  sim.beginRecent();
  sim.ctrl.handle(Command::serviceMode(true));
  sim.ctrl.handle(Command::setValve(Valve::FreshWater, true));
  sim.in.flowLpm = 2.0f;
  sim.run(100);
  ASSERT_VALVES(kFresh, sim);
  sim.run(899900);
  ASSERT_MODE(Mode::Service, sim);
  sim.run(100);
  ASSERT_MODE(Mode::Fault, sim);
  ASSERT_FAULT(FaultReason::InletOpenTooLong, sim);
  ASSERT_VALVES(kAllClosed, sim);
}

// ---------------------------------------------------------------------------
// Sicherheitslimits

void test_fault_when_inlet_open_too_long() {
  Sim sim;
  sim.beginRecent();
  sim.in.pressurized = false;
  sim.run(10000);
  ASSERT_MODE(Mode::Producing, sim);
  sim.run(899000, 500);
  ASSERT_MODE(Mode::Producing, sim);
  sim.run(1000, 500);
  ASSERT_MODE(Mode::Fault, sim);
  ASSERT_FAULT(FaultReason::InletOpenTooLong, sim);
  ASSERT_VALVES(kAllClosed, sim);

  TEST_ASSERT_FALSE(sim.ctrl.handle(Command::startFlush(Program::System)));
  TEST_ASSERT_FALSE(sim.ctrl.handle(Command::serviceMode(true)));
  sim.run(60000, 500);  // bleibt zu, auch wenn weiter Druck fehlt
  ASSERT_MODE(Mode::Fault, sim);

  // Fehler behoben, Druck wieder da. (Fehlt der Druck weiter, öffnet der Zulauf nach dem Quittieren sofort wieder.)
  sim.in.pressurized = true;
  TEST_ASSERT_TRUE(sim.ctrl.handle(Command::acknowledgeFault()));
  sim.run(100);
  ASSERT_MODE(Mode::Idle, sim);
  ASSERT_FAULT(FaultReason::None, sim);
}

void test_fault_when_no_pressure_after_draw() {
  Settings s;
  s.drawThresholdLpm = 1.0f;
  Sim sim;
  sim.beginRecent(s);
  sim.in.pressurized = false;
  sim.in.flowLpm = 3.0f;  // Entnahme
  sim.run(10000);
  ASSERT_MODE(Mode::Producing, sim);
  sim.run(480000, 500);  // 8 min Entnahme sind erlaubt
  ASSERT_MODE(Mode::Producing, sim);

  sim.in.flowLpm = 0.5f;  // Entnahme vorbei, Druckaufbau
  sim.run(299500, 500);
  ASSERT_MODE(Mode::Producing, sim);
  sim.run(500, 500);
  ASSERT_MODE(Mode::Fault, sim);
  ASSERT_FAULT(FaultReason::PressureBuildTimeout, sim);
}

void test_long_draw_hits_inlet_limit() {
  Settings s;
  s.drawThresholdLpm = 1.0f;
  Sim sim;
  sim.beginRecent(s);
  sim.in.pressurized = false;
  sim.in.flowLpm = 3.0f;
  sim.run(10000);
  sim.run(899000, 500);
  ASSERT_MODE(Mode::Producing, sim);
  sim.run(1000, 500);
  ASSERT_FAULT(FaultReason::InletOpenTooLong, sim);
}

void test_draw_detection_off_without_threshold() {
  Sim sim;
  sim.beginRecent();  // Entnahme-Schwelle 0 = aus
  sim.in.pressurized = false;
  sim.in.flowLpm = 0.5f;
  sim.run(10000);
  sim.run(600000, 500);
  ASSERT_MODE(Mode::Producing, sim);
}

// ---------------------------------------------------------------------------
// Warnungen

void test_warning_no_inlet_flow() {
  Sim sim;
  sim.beginRecent();
  sim.in.pressurized = false;
  sim.run(10000);
  ASSERT_MODE(Mode::Producing, sim);
  sim.run(59900);
  TEST_ASSERT_EQUAL_HEX8(0, sim.status().warnings);
  sim.run(100);
  TEST_ASSERT_EQUAL_HEX8(kWarnNoInletFlow, sim.status().warnings);

  sim.in.flowLpm = 2.0f;
  sim.run(100);
  TEST_ASSERT_EQUAL_HEX8(0, sim.status().warnings);
}

void test_warning_valve_leak() {
  Sim sim;
  sim.beginRecent();
  sim.in.flowLpm = 0.5f;  // Zulauf zu, trotzdem Durchfluss
  sim.run(10000);         // erst 10 s nach dem Schließen wird ausgewertet
  sim.run(29900);
  TEST_ASSERT_EQUAL_HEX8(0, sim.status().warnings);
  sim.run(100);
  TEST_ASSERT_EQUAL_HEX8(kWarnValveLeak, sim.status().warnings);
  ASSERT_MODE(Mode::Idle, sim);  // nur Warnung, keine Abschaltung

  sim.in.flowLpm = 0.0f;
  sim.run(100);
  TEST_ASSERT_EQUAL_HEX8(0, sim.status().warnings);
}

int main(int, char**) {
  UNITY_BEGIN();

  RUN_TEST(test_settings_sanitize_clamps_invalid_values);

  RUN_TEST(test_scheduler_interval_due_until_finished);
  RUN_TEST(test_scheduler_membrane_interval);
  RUN_TEST(test_scheduler_interval_zero_is_off);
  RUN_TEST(test_scheduler_fixed_time_once_per_day);
  RUN_TEST(test_scheduler_fixed_time_needs_valid_clock);
  RUN_TEST(test_scheduler_fixed_time_disabled);
  RUN_TEST(test_scheduler_midnight_is_a_valid_time);
  RUN_TEST(test_scheduler_late_evening_time_not_repeated_after_midnight);
  RUN_TEST(test_scheduler_catches_up_after_clock_jump);
  RUN_TEST(test_scheduler_changed_time_runs_again_same_day);

  RUN_TEST(test_starts_idle_with_valves_closed);
  RUN_TEST(test_startup_flush_skipped_after_recent_boot);
  RUN_TEST(test_startup_flush_runs_after_old_boot);
  RUN_TEST(test_startup_flush_runs_without_boot_record);
  RUN_TEST(test_startup_flush_runs_offline_after_waiting_for_clock);

  RUN_TEST(test_pressure_control_respects_hold_time);

  RUN_TEST(test_system_flush_sequence);
  RUN_TEST(test_membrane_flush_takes_five_minutes);
  RUN_TEST(test_flush_interrupts_production);
  RUN_TEST(test_request_during_flush_is_queued_and_duplicate_rejected);
  RUN_TEST(test_abort_flush);
  RUN_TEST(test_fixed_flush_time_starts_system_flush);

  RUN_TEST(test_service_mode_manual_valves_and_queue);
  RUN_TEST(test_entering_service_aborts_flush);
  RUN_TEST(test_service_mode_inlet_limit);

  RUN_TEST(test_fault_when_inlet_open_too_long);
  RUN_TEST(test_fault_when_no_pressure_after_draw);
  RUN_TEST(test_long_draw_hits_inlet_limit);
  RUN_TEST(test_draw_detection_off_without_threshold);

  RUN_TEST(test_warning_no_inlet_flow);
  RUN_TEST(test_warning_valve_leak);

  return UNITY_END();
}
