#pragma once

#define LED_COUNT 8
#define LED_PIN 23

enum LedState {
  staticRed,
  spinningRed,
  staticRainbow,
  staticGreen,
  staticBlue,
  spinningBlue,
  spinningRainbow
};

void ledTask(void *parameter);

void setupLeds();
