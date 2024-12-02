#include <ArduinoSTL.h>
#include <LiquidCrystal_I2C.h>

class MenuManager {
private:
  std::vector<MenuEntry> items;
  int currentMenu;
  unsigned long startMillisIdle;
  unsigned long currentMillisIdle;
  bool open;
  TaskHandle_t taskhandle;
  LiquidCrystal_I2C &lcd;
  State &state;

  void display();
  void handleOk();
  void task();
  static void taskWrapper(void* parameters);
public:
  MenuManager(std::initializer_list<MenuEntry> menus, LiquidCrystal_I2C &lcd, State &state);
  void next();
  void prev();
  void reset();
  void goTo(int menu);
  void open();
  void close(); 
  bool state();
  MenuEntry getCurrentMenu();
};