#pragma once
#include "../state.h"
#include "entry.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Vector.h>

class MenuManager {
private:
  Vector<MenuEntry> items;
  int currentMenu;
  unsigned long startMillisIdle;
  unsigned long currentMillisIdle;
  bool isOpen;
  TaskHandle_t taskhandle;
  LiquidCrystal_I2C &lcd;
  State &state;

  void display();
  void handleOk();
  void task();

public:
  MenuManager(std::initializer_list<MenuEntry> menus, LiquidCrystal_I2C &lcd,
              State &state);
  bool next();
  bool prev();
  void reset();
  void goTo(int menu);
  void open();
  void close();
  inline bool openState() { return this->isOpen; };
  inline MenuEntry getCurrentMenu() { return this->items[this->currentMenu];};
};
