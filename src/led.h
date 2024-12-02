#pragma once
#include <FastLED.h>

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

#define LED_COUNT 8

CRGB leds[LED_COUNT];