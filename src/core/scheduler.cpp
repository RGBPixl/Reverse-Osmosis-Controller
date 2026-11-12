#include "scheduler.h"

namespace core {

namespace {

constexpr uint16_t kMinutesPerDay = 24 * 60;
constexpr uint32_t kMsPerHour = 3600UL * 1000UL;

bool intervalDue(uint8_t intervalH, uint32_t lastMs, uint32_t nowMs) {
  return intervalH > 0 && static_cast<uint32_t>(nowMs - lastMs) >= intervalH * kMsPerHour;
}

}  // namespace

void Scheduler::begin(uint32_t nowMs) {
  lastSystemMs_ = nowMs;
  lastMembraneMs_ = nowMs;
  for (FixedRun& run : fixedRuns_) {
    run = FixedRun();
  }
}

uint8_t Scheduler::tick(const config::Settings& settings, const WallClock& clock, uint32_t nowMs) {
  uint8_t due = 0;

  if (intervalDue(settings.systemFlushIntervalH, lastSystemMs_, nowMs)) {
    due |= programBit(Program::System);
  }
  if (intervalDue(settings.membraneFlushIntervalH, lastMembraneMs_, nowMs)) {
    due |= programBit(Program::Membrane);
  }

  if (!clock.valid) {
    return due;
  }

  for (size_t i = 0; i < config::kFlushTimeCount; ++i) {
    const config::FlushTime& time = settings.flushTimes[i];
    if (!time.enabled) {
      continue;
    }
    const uint16_t target = static_cast<uint16_t>(time.hour * 60 + time.minute);
    const uint16_t now = clock.minuteOfDay;
    const uint16_t sinceTarget = static_cast<uint16_t>((now + kMinutesPerDay - target) % kMinutesPerDay);
    if (sinceTarget >= kCatchUpMinutes) {
      continue;
    }
    // Liegt die Zielzeit kurz vor Mitternacht und es ist schon nach Mitternacht,
    // gehört die Spülung noch zum Vortag.
    const uint32_t occurrenceDay = now < target ? clock.day - 1 : clock.day;

    FixedRun& run = fixedRuns_[i];
    if (run.ran && run.day == occurrenceDay && run.minuteOfDay == target) {
      continue;
    }
    run.ran = true;
    run.day = occurrenceDay;
    run.minuteOfDay = target;
    due |= programBit(Program::System);
  }

  return due;
}

void Scheduler::programFinished(Program program, uint32_t nowMs) {
  switch (program) {
    case Program::System: lastSystemMs_ = nowMs; break;
    case Program::Membrane: lastMembraneMs_ = nowMs; break;
    case Program::Startup: break;
  }
}

}  // namespace core
