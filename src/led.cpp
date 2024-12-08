// FastLED Warnung ignorieren
#define FASTLED_INTERNAL
#include <FastLED.h>

#include "led.h"

CRGB leds[LED_COUNT];

void setupLeds() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(30);
}

void ledTask(void *parameter) {
  // LedState *state = (LedState *)parameter;
  while (true) {
    switch (*(LedState *)parameter) {
    case staticRed:
      fill_solid(leds, LED_COUNT, CRGB::Red);
      FastLED.show();
      break;
    case staticGreen:
      fill_solid(leds, LED_COUNT, CRGB::Green); // CRGB::Lime für ein intesiveres Grün
      FastLED.show();
      break;
    case staticBlue:
      fill_solid(leds, LED_COUNT, CRGB::Blue);
      FastLED.show();
      break;
    case staticRainbow:
      fill_rainbow(leds, LED_COUNT, 0, 32);
      FastLED.show();
      break;
    case spinningRed:
      for (int i = 0; i < LED_COUNT; i++) {
        fadeToBlackBy(leds, LED_COUNT, 100);
        leds[i] = CRGB::Red;
        FastLED.show();
        delay(100);
      }
      break;
    case spinningBlue:
      for (int i = 0; i < LED_COUNT; i++) {
        fadeToBlackBy(leds, LED_COUNT, 100);
        leds[i] = CRGB::Blue;
        FastLED.show();
        delay(100);
      }
      break;
    case spinningRainbow:
      for (int i = LED_COUNT - 1; i >= 0; i--) {
        fill_rainbow(leds, LED_COUNT, i * 32, 32);
        FastLED.show();
        delay(100);
      }
      break;
    }
    vTaskDelay(1); // Leichtes Yielding, um den Watchdog nicht zu triggern
  }
}