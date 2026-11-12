#pragma once
// Hilfen rund um die Uhrzeit. Ohne Arduino-Abhängigkeit, damit die Steuerlogik testbar bleibt.

#include <stdint.h>
#include <time.h>

namespace util {

// Alles vor dem 01.01.2024 gilt als "Uhr noch nicht gestellt".
constexpr time_t kMinValidEpoch = 1704067200;

// Zeitzone Deutschland mit automatischer Sommer-/Winterzeit.
constexpr const char* kTimezone = "CET-1CEST,M3.5.0,M10.5.0/3";

inline bool timeValid(time_t now) { return now >= kMinValidEpoch; }
inline bool timeValid() { return timeValid(time(nullptr)); }

struct CivilDate {
  int32_t year;
  uint8_t month;  // 1..12
  uint8_t day;    // 1..31
};

// Fortlaufende Tagesnummer (Tage seit 01.01.1970) für ein Datum und zurück.
// Algorithmus nach Howard Hinnant, "chrono-Compatible Low-Level Date Algorithms".
inline int32_t daysFromCivil(int32_t year, uint32_t month, uint32_t day) {
  year -= month <= 2 ? 1 : 0;
  const int32_t era = (year >= 0 ? year : year - 399) / 400;
  const uint32_t yearOfEra = static_cast<uint32_t>(year - era * 400);
  const uint32_t dayOfYear = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
  const uint32_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + static_cast<int32_t>(dayOfEra) - 719468;
}

inline CivilDate civilFromDays(int32_t days) {
  days += 719468;
  const int32_t era = (days >= 0 ? days : days - 146096) / 146097;
  const uint32_t dayOfEra = static_cast<uint32_t>(days - era * 146097);
  const uint32_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  const uint32_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const uint32_t mp = (5 * dayOfYear + 2) / 153;
  const uint32_t day = dayOfYear - (153 * mp + 2) / 5 + 1;
  const uint32_t month = mp < 10 ? mp + 3 : mp - 9;
  CivilDate date;
  date.year = static_cast<int32_t>(yearOfEra) + era * 400 + (month <= 2 ? 1 : 0);
  date.month = static_cast<uint8_t>(month);
  date.day = static_cast<uint8_t>(day);
  return date;
}

}  // namespace util
