#pragma once
#include "state.h"
#include "error_handling.h"
#include <Arduino.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>
#include <time.h>

extern LiquidCrystal_I2C *lcd;
extern State *state;
extern ErrorType error_state;
extern Preferences preferences;