#pragma once
// Zwei DS18B20 am OneWire-Bus, nicht blockierend:
// Messung anstoßen, später abholen, statt bei jedem Durchlauf ~750 ms zu warten.
// update() regelmäßig aufrufen. Index 0 = vor Filter, Index 1 = nach Filter.

#include <DallasTemperature.h>
#include <OneWire.h>
#include <stdint.h>

namespace hal {

class Temperature {
 public:
  static constexpr uint8_t kMaxSensors = 2;
  static constexpr uint32_t kIntervalMs = 5000;        // Abstand zwischen zwei Messungen
  static constexpr uint32_t kRediscoverMs = 30000;     // erneute Suche, wenn kein Sensor gefunden
  static constexpr uint8_t kFailuresUntilInvalid = 3;  // so viele Fehlmessungen, bis ein Wert ungültig wird

  Temperature();

  void begin(uint32_t nowMs);
  void update(uint32_t nowMs);

  uint8_t count() const { return count_; }
  bool valid(uint8_t index) const { return index < count_ && valid_[index]; }
  float celsius(uint8_t index) const { return index < kMaxSensors ? celsius_[index] : 0.0f; }

 private:
  void discover(uint32_t nowMs);
  void readResults(uint32_t nowMs);

  // Reihenfolge wichtig: oneWire_ muss vor sensors_ konstruiert werden.
  OneWire oneWire_;
  DallasTemperature sensors_;

  DeviceAddress addresses_[kMaxSensors];
  uint8_t count_ = 0;
  float celsius_[kMaxSensors] = {0.0f, 0.0f};
  bool valid_[kMaxSensors] = {false, false};
  uint8_t failures_[kMaxSensors] = {0, 0};

  bool converting_ = false;
  uint32_t conversionMs_ = 750;
  uint32_t conversionStartMs_ = 0;
  uint32_t nextActionMs_ = 0;
};

}  // namespace hal
