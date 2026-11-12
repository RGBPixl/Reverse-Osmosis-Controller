#include "flow_meter.h"

#include <Arduino.h>

#include "../config/pins.h"
#include "../util/log.h"

namespace hal {

namespace {

volatile uint32_t gPulses = 0;
portMUX_TYPE gPulseMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onPulse() {
  portENTER_CRITICAL_ISR(&gPulseMux);
  gPulses = gPulses + 1;
  portEXIT_CRITICAL_ISR(&gPulseMux);
}

uint32_t readPulses() {
  portENTER_CRITICAL(&gPulseMux);
  const uint32_t pulses = gPulses;
  portEXIT_CRITICAL(&gPulseMux);
  return pulses;
}

}  // namespace

void FlowMeter::begin(double totalLiters, float litersPerPulse, uint32_t nowMs) {
  totalLiters_ = totalLiters;
  litersPerPulse_ = litersPerPulse;
  lastPulses_ = windowStartPulses_ = readPulses();
  lastPulseMs_ = windowStartMs_ = nowMs;

  pinMode(pins::kFlowSensor, INPUT);  // GPIO 34 hat keinen internen Pull-up
  attachInterrupt(digitalPinToInterrupt(pins::kFlowSensor), onPulse, FALLING);
  util::logf("Durchfluss", "Initialisiert, Zaehlerstand %.2f L", totalLiters_);
}

void FlowMeter::update(uint32_t nowMs) {
  const uint32_t pulses = readPulses();
  const uint32_t delta = pulses - lastPulses_;
  if (delta > 0) {
    totalLiters_ += static_cast<double>(delta) * litersPerPulse_;
    lastPulses_ = pulses;
    lastPulseMs_ = nowMs;
  }

  const uint32_t elapsed = nowMs - windowStartMs_;
  if (elapsed >= kRateWindowMs) {
    const uint32_t windowPulses = pulses - windowStartPulses_;
    litersPerMinute_ = static_cast<float>(windowPulses) * litersPerPulse_ * 60000.0f / static_cast<float>(elapsed);
    windowStartMs_ = nowMs;
    windowStartPulses_ = pulses;
  }
}

}  // namespace hal
