#pragma once
// Einheitliches Logging auf Serial.
// Format: "[12:34:56] [Tag] Text", solange die Uhr nicht gestellt ist "[+123s] [Tag] Text".
// Wartet nie auf die Uhrzeit (die alte Firmware hat dafür bei jedem Relais bis zu 5 s blockiert).

namespace util {

void logf(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

}  // namespace util
