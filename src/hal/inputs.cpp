#include "inputs.h"

#include <Arduino.h>

#include "../config/pins.h"
#include "../util/log.h"

namespace hal {

namespace {

constexpr uint8_t kButtonPins[kButtonCount] = {
    pins::kButtonOk,     // Button::Ok
    pins::kButtonLeft,   // Button::Left
    pins::kButtonRight,  // Button::Right
};

bool readButton(uint8_t index) { return digitalRead(kButtonPins[index]) == LOW; }
bool readPressure() { return digitalRead(pins::kPressureSwitch) == HIGH; }

}  // namespace

void Inputs::begin(uint32_t nowMs) {
  for (uint8_t i = 0; i < kButtonCount; ++i) {
    pinMode(kButtonPins[i], INPUT_PULLUP);
  }
  pinMode(pins::kPressureSwitch, INPUT);  // GPIO 35 hat keinen internen Pull-up

  for (uint8_t i = 0; i < kButtonCount; ++i) {
    buttons_[i].debouncer.reset(readButton(i), nowMs);
  }
  pressure_.reset(readPressure(), nowMs);
  util::logf("Eingaenge", "Initialisiert, Druck %s", pressure_.state() ? "da" : "weg");
}

void Inputs::update(uint32_t nowMs) {
  for (uint8_t i = 0; i < kButtonCount; ++i) {
    ButtonState& b = buttons_[i];
    if (b.debouncer.update(readButton(i), nowMs)) {
      if (b.debouncer.state()) {
        b.pressedAtMs = nowMs;
        b.longFired = false;
      } else if (!b.longFired) {
        b.pending = ButtonEvent::Short;
      }
    } else if (b.debouncer.state() && !b.longFired &&
               static_cast<uint32_t>(nowMs - b.pressedAtMs) >= kLongPressMs) {
      b.longFired = true;
      b.pending = ButtonEvent::Long;
    }
  }

  if (pressure_.update(readPressure(), nowMs)) {
    util::logf("Eingaenge", "Druck %s", pressure_.state() ? "da" : "weg");
  }
}

ButtonEvent Inputs::takeEvent(Button button) {
  ButtonState& b = buttons_[static_cast<uint8_t>(button)];
  const ButtonEvent event = b.pending;
  b.pending = ButtonEvent::None;
  return event;
}

}  // namespace hal
