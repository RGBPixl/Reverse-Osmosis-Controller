#pragma once
// Hilfen rund um die Uhrzeit. Ohne Arduino-Abhängigkeit, damit die Steuerlogik testbar bleibt.

#include <time.h>

namespace util {

// Alles vor dem 01.01.2024 gilt als "Uhr noch nicht gestellt".
constexpr time_t kMinValidEpoch = 1704067200;

// Zeitzone Deutschland mit automatischer Sommer-/Winterzeit.
constexpr const char* kTimezone = "CET-1CEST,M3.5.0,M10.5.0/3";

inline bool timeValid(time_t now) { return now >= kMinValidEpoch; }
inline bool timeValid() { return timeValid(time(nullptr)); }

}  // namespace util
