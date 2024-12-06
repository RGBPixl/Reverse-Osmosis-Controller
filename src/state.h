#pragma once
#include "led.h"
#include <Arduino.h>
#include <Preferences.h>

class State {
public:
  int currentLedTest = 0;
  int currentResetState = 0;
  String shortStatus = "BOOT"; // max. 8 Zeichen

  int flowImpulseCount = 0;
  float flowLiters;
  bool fillContainer = false;
  bool flushMembrane = false;
  bool flushSystem = false;

  int testState = 0;
  double temp1;
  double temp2;

  int hourOfDay;

  LedState ledState;

  volatile bool okPressed = false;
  volatile bool lPressed = false;
  volatile bool rPressed = false;

  Preferences preferences;
  int intervallFlushSystem;
  int intervallFlushMembrane;

  State();
};
