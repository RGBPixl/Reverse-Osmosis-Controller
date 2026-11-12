#include "settings.h"

namespace config {

namespace {

template <typename T>
bool clampTo(T& value, T min, T max) {
  if (value < min) {
    value = min;
    return true;
  }
  if (value > max) {
    value = max;
    return true;
  }
  return false;
}

// Auch für NaN: Ungültige Werte fallen auf den Standard zurück.
bool clampFloat(float& value, float min, float max, float fallback) {
  if (!(value >= min && value <= max)) {
    value = fallback;
    return true;
  }
  return false;
}

template <size_t N>
bool terminate(char (&text)[N]) {
  if (text[N - 1] != '\0') {
    text[N - 1] = '\0';
    return true;
  }
  return false;
}

}  // namespace

bool sanitize(Settings& s) {
  const Settings defaults;
  bool changed = false;

  changed |= clampTo<uint8_t>(s.systemFlushIntervalH, 0, kMaxFlushIntervalH);
  changed |= clampTo<uint8_t>(s.membraneFlushIntervalH, 0, kMaxFlushIntervalH);
  for (FlushTime& t : s.flushTimes) {
    changed |= clampTo<uint8_t>(t.hour, 0, 23);
    changed |= clampTo<uint8_t>(t.minute, 0, 59);
  }

  changed |= clampTo<uint16_t>(s.systemFlushStepS, kMinSystemFlushStepS, kMaxSystemFlushStepS);
  changed |= clampTo<uint16_t>(s.membraneFlushS, kMinMembraneFlushS, kMaxMembraneFlushS);
  changed |= clampTo<uint16_t>(s.pressureBuildMaxS, kMinPressureBuildS, kMaxPressureBuildS);
  changed |= clampTo<uint16_t>(s.inletMaxOpenS, kMinInletOpenS, kMaxInletOpenS);

  changed |= clampFloat(s.drawThresholdLpm, 0.0f, kMaxDrawThresholdLpm, defaults.drawThresholdLpm);
  changed |= clampFloat(s.litersPerPulse, kMinLitersPerPulse, kMaxLitersPerPulse, defaults.litersPerPulse);

  if (s.mqttPort == 0) {
    s.mqttPort = defaults.mqttPort;
    changed = true;
  }
  changed |= terminate(s.mqttServer);
  changed |= terminate(s.mqttUser);
  changed |= terminate(s.mqttPassword);

  return changed;
}

}  // namespace config
