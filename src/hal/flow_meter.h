#pragma once
// Durchflusssensor im Zulauf: Impulse per Interrupt, daraus Gesamtmenge und L/min.
// update() regelmäßig aufrufen, etwa alle 10 ms. Speichern im NVS macht die Ablaufsteuerung.

#include <stdint.h>

namespace hal {

class FlowMeter {
 public:
  static constexpr uint32_t kRateWindowMs = 1000;

  void begin(double totalLiters, float litersPerPulse, uint32_t nowMs);
  void update(uint32_t nowMs);

  void setLitersPerPulse(float litersPerPulse) { litersPerPulse_ = litersPerPulse; }

  double totalLiters() const { return totalLiters_; }
  void resetTotal() { totalLiters_ = 0.0; }

  // Durchfluss der letzten vollen Sekunde
  float litersPerMinute() const { return litersPerMinute_; }

  // Zeit seit dem letzten Impuls (Auflösung = Aufrufabstand von update()).
  // Vor dem ersten Impuls: Zeit seit begin().
  uint32_t msSinceLastPulse(uint32_t nowMs) const { return nowMs - lastPulseMs_; }

 private:
  float litersPerPulse_ = 0.0f;
  double totalLiters_ = 0.0;  // double: float wäre bei großen Summen zu ungenau
  float litersPerMinute_ = 0.0f;
  uint32_t lastPulses_ = 0;
  uint32_t lastPulseMs_ = 0;
  uint32_t windowStartMs_ = 0;
  uint32_t windowStartPulses_ = 0;
};

}  // namespace hal
