#pragma once
#include "../state.h"
#include "../error_handling.h"
#include "../global_vars.h"
#include "entry.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Vector.h>

class MenuManager {
public:
  MenuManager(std::initializer_list<MenuEntry> menus);
  inline ~MenuManager() { Serial.print("MenuManager destroyed"); };

private:
  bool isOpen;
  int16_t currentMenu;
  uint64_t startMillisIdle;
  uint64_t currentMillisIdle;
  TaskHandle_t taskhandle;
//  LiquidCrystal_I2C &lcd;
//  State &state;
  Vector<MenuEntry> entries;

private:
  void display();
  void handleOk();
  void task();

public:
  inline void reset() {this->goTo(0);};
  inline bool openState() { return this->isOpen; };
  inline MenuEntry getCurrentMenu() { return this->entries[this->currentMenu];};
  
  bool next();
  bool prev();
  void goTo(int menu);
  void open();
  void close();
};
