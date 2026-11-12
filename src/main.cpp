// Reverse-Osmosis-Controller
// Aufbau und Verhalten: docs/refactoring-konzept.md

#include <Arduino.h>

#include "app/app.h"

namespace {

app::App application;

}  // namespace

void setup() {
  Serial.begin(115200);
  application.setup();
}

void loop() { application.loop(); }
