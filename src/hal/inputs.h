#pragma once
// Taster und Druckschalter, entprellt und gepollt (keine Interrupts).
// update() regelmäßig aufrufen, etwa alle 10 ms.

#include <stdint.h>

#include "../util/debouncer.h"

namespace hal {

enum class Button : uint8_t { Ok = 0, Left = 1, Right = 2 };
constexpr uint8_t kButtonCount = 3;

enum class ButtonEvent : uint8_t {
  None,
  Short,  // losgelassen nach weniger als kLongPressMs
  Long,   // kLongPressMs lang gehalten (kommt einmal, noch während des Haltens)
};

class Inputs {
 public:
  static constexpr uint32_t kButtonDebounceMs = 30;
  static constexpr uint32_t kLongPressMs = 1000;
  static constexpr uint32_t kPressureDebounceMs = 2000;

  void begin(uint32_t nowMs);
  void update(uint32_t nowMs);

  // Holt das letzte Ereignis eines Tasters ab und löscht es.
  ButtonEvent takeEvent(Button button);

  // Entprellter Druckschalter: true = Druck da.
  bool pressurized() const { return pressure_.state(); }

 private:
  struct ButtonState {
    util::Debouncer debouncer{kButtonDebounceMs};
    uint32_t pressedAtMs = 0;
    bool longFired = false;
    ButtonEvent pending = ButtonEvent::None;
  };

  ButtonState buttons_[kButtonCount];
  util::Debouncer pressure_{kPressureDebounceMs};
};

}  // namespace hal
