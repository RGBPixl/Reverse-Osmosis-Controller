#pragma once
#include "led.h"
#include <Arduino.h>
#include <Preferences.h>

class State {
public: // constructor
  inline State() : 
      ledState(LedState::spinningRed)
    , shortStatus("BOOT")
    , flushMembrane(false) // false & 0 inits eig irrelevant da das die Standardwerte sind, sicherheitshalber trotzdem ;D
    , flushSystem(false)
    , fillContainer(false)
    , currentLedTest(0)
    , currentResetState(0)
    , flowImpulseCount(0)
    , testState(0)
   {};

public: // variables
  bool flushMembrane;
  bool flushSystem;
  bool fillContainer;
 
  char shortStatus[9] = {0}; // max. 8 Zeichen,
  // Zeichen 9 muss IMMER '\0' sein -> String-Terminator

  int currentLedTest;
  int currentResetState;
  int flowImpulseCount;
  int testState;
  int hourOfDay;
  int intervallFlushSystem;
  int intervallFlushMembrane;
  float flowLiters;
  double temp1;
  double temp2;

  LedState ledState;
  Preferences preferences;

public: // volatile = Do not optimize this away!
  volatile bool okPressed = false;
  volatile bool lPressed = false;
  volatile bool rPressed = false;


};
