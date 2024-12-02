#include "state.h"
#include "led.h"

State::State() {
    this->ledState = LedState::spinningRed;
}