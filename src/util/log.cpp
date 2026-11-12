#include "log.h"

#include <Arduino.h>
#include <stdarg.h>
#include <time.h>

#include "clock.h"

namespace util {

void logf(const char* tag, const char* fmt, ...) {
  char prefix[16];
  const time_t now = time(nullptr);
  if (timeValid(now)) {
    struct tm local;
    localtime_r(&now, &local);
    strftime(prefix, sizeof(prefix), "%H:%M:%S", &local);
  } else {
    snprintf(prefix, sizeof(prefix), "+%lus", static_cast<unsigned long>(millis() / 1000));
  }

  char message[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  Serial.printf("[%s] [%s] %s\n", prefix, tag, message);
}

}  // namespace util
