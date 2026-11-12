#include "temperature.h"

#include "../config/pins.h"
#include "../util/log.h"

namespace hal {

namespace {

// 85 °C ist der Einschaltwert des DS18B20 und kommt bei dieser Anlage real nicht vor.
bool plausible(float c) { return c != DEVICE_DISCONNECTED_C && c > -55.0f && c < 85.0f; }

bool due(uint32_t nowMs, uint32_t atMs) { return static_cast<int32_t>(nowMs - atMs) >= 0; }

}  // namespace

Temperature::Temperature() : oneWire_(pins::kOneWireBus), sensors_(&oneWire_) {}

void Temperature::begin(uint32_t nowMs) {
  sensors_.setWaitForConversion(false);
  discover(nowMs);
}

void Temperature::discover(uint32_t nowMs) {
  sensors_.begin();
  const uint8_t found = sensors_.getDeviceCount();
  count_ = 0;
  for (uint8_t i = 0; i < found && count_ < kMaxSensors; ++i) {
    if (sensors_.getAddress(addresses_[count_], i)) {
      valid_[count_] = false;
      failures_[count_] = 0;
      ++count_;
    }
  }
  conversionMs_ = sensors_.millisToWaitForConversion() + 20;
  converting_ = false;
  nextActionMs_ = count_ > 0 ? nowMs : nowMs + kRediscoverMs;

  if (count_ < kMaxSensors) {
    util::logf("Temperatur", "%u von %u Sensoren gefunden", static_cast<unsigned>(count_),
               static_cast<unsigned>(kMaxSensors));
  } else {
    util::logf("Temperatur", "Beide Sensoren gefunden");
  }
}

void Temperature::update(uint32_t nowMs) {
  if (count_ == 0) {
    if (due(nowMs, nextActionMs_)) {
      discover(nowMs);
    }
    return;
  }

  if (!converting_) {
    if (due(nowMs, nextActionMs_)) {
      sensors_.requestTemperatures();  // kehrt dank setWaitForConversion(false) sofort zurück
      converting_ = true;
      conversionStartMs_ = nowMs;
    }
    return;
  }

  if (static_cast<uint32_t>(nowMs - conversionStartMs_) >= conversionMs_) {
    readResults(nowMs);
  }
}

void Temperature::readResults(uint32_t nowMs) {
  converting_ = false;
  nextActionMs_ = nowMs + kIntervalMs;

  bool anyValid = false;
  for (uint8_t i = 0; i < count_; ++i) {
    const float c = sensors_.getTempC(addresses_[i]);
    if (plausible(c)) {
      celsius_[i] = c;
      valid_[i] = true;
      failures_[i] = 0;
    } else if (failures_[i] < kFailuresUntilInvalid && ++failures_[i] == kFailuresUntilInvalid) {
      valid_[i] = false;
      util::logf("Temperatur", "Sensor %u liefert keine gueltigen Werte mehr", static_cast<unsigned>(i));
    }
    anyValid = anyValid || valid_[i];
  }

  // Liefert kein Sensor mehr etwas, Bus neu durchsuchen (z. B. nach Wackelkontakt).
  if (!anyValid) {
    bool allFailed = true;
    for (uint8_t i = 0; i < count_; ++i) {
      allFailed = allFailed && failures_[i] >= kFailuresUntilInvalid;
    }
    if (allFailed) {
      count_ = 0;
      nextActionMs_ = nowMs + kRediscoverMs;
    }
  }
}

}  // namespace hal
