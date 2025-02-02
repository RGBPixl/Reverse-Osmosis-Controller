#pragma once
#include "../state.h"
#include "../error_handling.h"
#include "../global_vars.h"
#include "entry.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <vector>

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
  std::vector<MenuEntry> entries;

private:
  void display();
  void handleOk();
  void task();

public:
  inline void reset() { entries[currentMenu].setPage(0); };
  inline bool openState() { return isOpen; };
  inline MenuEntry &getCurrentMenu() { return entries[currentMenu]; };

  void next();
  void prev();
  void setMenu(int menu);
  void open();
  void close();
};
