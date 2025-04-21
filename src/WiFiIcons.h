#pragma once
#include <Arduino.h>

// Kein Signal
byte wifiLevel0[8] = {
    B00000,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000
};

// Schwaches Signal
byte wifiLevel1[8] = {
    B00000,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000,
    B00100,
    B00000
};

// Mittleres Signal
byte wifiLevel2[8] = {
    B00000,
    B00000,
    B00000,
    B00100,
    B01010,
    B00000,
    B00100,
    B00000
};

// Starkes Signal
byte wifiLevel3[8] = {
    B00100,
    B01010,
    B10001,
    B00100,
    B01010,
    B00000,
    B00100,
    B00000  
};

// Disconnected (durchgestrichen)
byte wifiDisconnected[8] = {
    B00100,
    B01110,
    B10101,
    B00100,
    B01110,
    B00100,
    B00100,
    B00100
};
