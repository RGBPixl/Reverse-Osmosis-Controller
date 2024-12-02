#include "../state.h"
#include "../shitty_vec.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

class MenuManager {
private:
  ShittyVec<MenuEntry> items;
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
  MenuManager(std::initializer_list<MenuEntry> menus, LiquidCrystal_I2C &lcd, State &state);
  bool next();
  bool prev();
  void reset();
  void goTo(int menu);
  void open();
  void close(); 
  bool openState();
  MenuEntry getCurrentMenu();
};