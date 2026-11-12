#pragma once
// Umrechnung "Helligkeit, wie das Auge sie sieht" -> PWM-Einschaltdauer.
// Das Auge unterscheidet dunkle Stufen viel stärker als helle: 25 % Einschaltdauer wirken etwa halb so hell.
// Mit Gamma 2,2 entsprechen 25 % wahrgenommene Helligkeit rund 5 % Einschaltdauer.

#include <math.h>
#include <stdint.h>

namespace ui {

// percent 0..100 -> Einschaltdauer 0..255 (8 Bit). Alles über 0 % bleibt mindestens 1, also sichtbar.
inline uint8_t perceivedToDuty(uint8_t percent) {
  if (percent == 0) return 0;
  if (percent >= 100) return 255;
  const float duty = 255.0f * powf(percent / 100.0f, 2.2f);
  return duty < 1.0f ? 1 : static_cast<uint8_t>(duty + 0.5f);
}

}  // namespace ui
