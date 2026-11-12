#pragma once
// Entprellt ein digitales Signal: Ein neuer Zustand gilt erst, wenn das Rohsignal
// stableMs lang unverändert war. Ohne Arduino-Abhängigkeit, damit es testbar bleibt.

#include <stdint.h>

namespace util {

class Debouncer {
 public:
  explicit Debouncer(uint32_t stableMs) : stableMs_(stableMs) {}

  // Setzt den Zustand ohne Wartezeit, z. B. beim Start.
  void reset(bool state, uint32_t nowMs) {
    state_ = candidate_ = state;
    candidateSinceMs_ = changedAtMs_ = nowMs;
  }

  // Gibt true zurück, wenn sich der entprellte Zustand gerade geändert hat.
  bool update(bool raw, uint32_t nowMs) {
    if (raw != candidate_) {
      candidate_ = raw;
      candidateSinceMs_ = nowMs;
    }
    if (candidate_ != state_ && static_cast<uint32_t>(nowMs - candidateSinceMs_) >= stableMs_) {
      state_ = candidate_;
      changedAtMs_ = nowMs;
      return true;
    }
    return false;
  }

  bool state() const { return state_; }
  uint32_t changedAtMs() const { return changedAtMs_; }

 private:
  uint32_t stableMs_;
  bool state_ = false;
  bool candidate_ = false;
  uint32_t candidateSinceMs_ = 0;
  uint32_t changedAtMs_ = 0;
};

}  // namespace util
