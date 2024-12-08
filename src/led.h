#pragma once

#define LED_COUNT 8
#define LED_PIN 23

enum LedState {
  staticRed,
  staticGreen,
  staticBlue,
  staticRainbow,
  spinningRed,
  spinningBlue,
  spinningRainbow
};

void setupLeds();
void ledTask(void *parameter);