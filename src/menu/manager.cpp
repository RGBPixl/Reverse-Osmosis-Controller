#include "manager.h"
#include "entry.h"
#include "page.h"
#include "../state.h"
#include <Arduino.h>
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

  void display() {
    switch (this->getCurrentMenu().getCurrentPage()) {

    case pageTemp:
      lcd.setCursor(0, 0);
      lcd.print("Temperaturen");
      lcd.setCursor(0, 1);
      lcd.print("              ->");
      break;

    case pageFlow:
      lcd.setCursor(0, 0);
      lcd.print("Durchfluss");
      lcd.setCursor(0, 1);
      lcd.print("              ->");
      break;

    case pageSensor:
      lcd.setCursor(0, 0);
      lcd.print("Sonst. Sensoren");
      lcd.setCursor(0, 1);
      lcd.print("              ->");
      break;

    case pageFunction:
      lcd.setCursor(0, 0);
      lcd.print("Funktionen");
      lcd.setCursor(0, 1);
      lcd.print("              ->");
      break;

    case pageRelayStatus:
      lcd.setCursor(0, 0);
      lcd.print("Status Relais");
      lcd.setCursor(0, 1);
      lcd.print("              ->");
      break;

    case pageTest:
      lcd.setCursor(0, 0);
      lcd.print("Test");
      lcd.setCursor(0, 1);
      lcd.print("              ->");
      break;

    case pageTemp1:
      lcd.setCursor(0, 0);
      lcd.print("Vor Filter Temp");
      lcd.setCursor(0, 1);
      lcd.print(String(state.temp1) + " \xDF"
                                "C");
      break;

    case pageTemp2:
      lcd.setCursor(0, 0);
      lcd.print("Nach Filter Temp");
      lcd.setCursor(0, 1);
      lcd.print(String(state.temp2) + " \xDF"
                                "C");
      break;

    case pageWaterTotal:
      lcd.setCursor(0, 0);
      lcd.print("Wasser Total:");
      lcd.setCursor(0, 1);
      lcd.print("9999 L");
      break;

    case pageSensorStatus:
      lcd.setCursor(0, 0);
      lcd.print("Flow Sensor RAW:");
      lcd.setCursor(0, 1);
      lcd.print("23 IMP/M");
      break;

    case pageOverflowSensor:
      lcd.setCursor(0, 0);
      lcd.print("Overflow Sensor");
      lcd.setCursor(0, 1);
      lcd.print("offen");
      break;

    case pageSensor5:
      lcd.setCursor(0, 0);
      lcd.print("Dummy Sensor 1");
      lcd.setCursor(0, 1);
      lcd.print("offen");
      break;

    case pageSensor6:
      lcd.setCursor(0, 0);
      lcd.print("Dummy Sensor 2");
      lcd.setCursor(0, 1);
      lcd.print("offen");
      break;

    case pageDisinfection:
      lcd.setCursor(0, 0);
      lcd.print("Desinfektion");
      lcd.setCursor(0, 1);
      lcd.print("--> Starten <--");
      break;

    case pageFlushMembrane:
      lcd.setCursor(0, 0);
      lcd.print("Membran Sp"
                "\xF5"
                "len");
      lcd.setCursor(0, 1);
      lcd.print("--> Starten <--");
      break;

    case pageFlushSystem:
      lcd.setCursor(0, 0);
      lcd.print("System Sp"
                "\xF5"
                "len");
      lcd.setCursor(0, 1);
      lcd.print("--> Starten <--");
      break;

    case pageFillContainer:
      lcd.setCursor(0, 0);
      lcd.print("Kanister f"
                "\xF5"
                "llen");
      lcd.setCursor(0, 1);
      lcd.print("--> Starten <--");
      break;

    case pageRelay1:
      lcd.setCursor(0, 0);
      lcd.print("Relais Zulauf");
      lcd.setCursor(0, 1);
      lcd.print("offen");
      break;

    case pageRelay2:
      lcd.setCursor(0, 0);
      lcd.print("Relais Membran");
      lcd.setCursor(0, 1);
      lcd.print("offen");
      break;

    case pageRelay3:
      lcd.setCursor(0, 0);
      lcd.print("Relais Abfluss");
      lcd.setCursor(0, 1);
      lcd.print("offen");
      break;

    case pageRelay4:
      lcd.setCursor(0, 0);
      lcd.print("Relais Kanister");
      lcd.setCursor(0, 1);
      lcd.print("offen");
      break;

    case pageRelay5:
      lcd.setCursor(0, 0);
      lcd.print("Dummy R5");
      lcd.setCursor(0, 1);
      lcd.print("offen");
      break;

    case pageRelay6:
      lcd.setCursor(0, 0);
      lcd.print("Dummy R6");
      lcd.setCursor(0, 1);
      lcd.print("offen");
      break;

    case pageLedRingTest:
      lcd.setCursor(0, 0);
      lcd.print("Test LED Ring");
      lcd.setCursor(0, 1);
      lcd.print("Status: " + String(state.currentLedTest));
      break;

    case pageFactoryReset:
      lcd.setCursor(0, 0);
      lcd.print("Factory Reset");
      lcd.setCursor(0, 1);
      lcd.print("---> RESET <---");
      break;

    case pageResetConfirm:
      lcd.setCursor(0, 0);
      lcd.print("RESET");
      lcd.setCursor(0, 1);
      lcd.print("bestaetigen");
      break;

    case pageResetSuccess:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("RESET");
      lcd.setCursor(0, 1);
      lcd.print("erfolgreich...");
      break;
    }
  }
  void handleOk() {
    switch (this->getCurrentMenu().getCurrentPage()) {
    case pageLedRingTest:
      state.currentLedTest++;
      if (state.currentLedTest > 6) {
        state.currentLedTest = 0;
      }
      state.curState = (LedState)state.currentLedTest;
      break;
    case pageFillContainer:
      state.fillContainer = true;
      break;
    case pageFlushMembrane:
      state.flushMembrane = true;
      break;
    case pageFlushSystem:
      state.flushSystem = true;
      break;
    case pageFactoryReset:
    case pageResetConfirm:
      state.currentResetState++;
      switch (state.currentResetState) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print("ERROR");
        delay(2000);
        break;
      case 1:
        this->goTo(5);
        break;
      case 2:
        if (!state.preferences.begin("Config", false)) {
          Serial.println("Failed to initialize NVS");
          return;
        }
        state.preferences.putInt("iFlushSystem", 8);
        state.intervallFlushSystem = 8;
        state.preferences.putInt("iFlushMembrane", 24);
        state.intervallFlushMembrane = 24;
        state.preferences.end();
        state.currentResetState = 0;
        this->goTo(7);
        this->getCurrentMenu().goTo(1);
        break;
      }
      break;
    }
    if (this->currentMenu == 0) {
      this->next();
    }
  }
  void task() {
    this->startMillisIdle = millis();
    this->reset();
    lcd.clear();
    while (true) {
      if (state.rPressed) {
        this->startMillisIdle = millis();
        this->getCurrentMenu().next();
        lcd.clear();
        state.rPressed = false;
      }
      if (state.lPressed) {
        this->startMillisIdle = millis();
        this->getCurrentMenu().reset();
        lcd.clear();
        state.lPressed = false;
      }
      if (state.okPressed) {
        this->startMillisIdle = millis();
        this->handleOk();
        lcd.clear();
        state.okPressed = false;
      }
      this->display();
      this->currentMillisIdle = millis();
      if (this->currentMillisIdle - this->startMillisIdle >= 10000) {
        break;
      }
      delay(500);
      vTaskDelay(1);
    }
    lcd.clear();
    state.currentResetState = 0;
    this->open = false;
    vTaskDelete(this->taskhandle);
  }
  static void taskWrapper(void* parameters) {
      MenuManager* instance = static_cast<MenuManager*>(parameters);
      instance->task();
  }
public:
  MenuManager(std::initializer_list<MenuEntry> menus, LiquidCrystal_I2C &lcd, State &state) : items(menus), lcd(lcd), state(state) {
    this->currentMenu = 0;
    this->startMillisIdle = millis();
    this->currentMillisIdle = millis();
    this->open = false;
  }
  void next() {
    this->currentMenu++;
    if (this->currentMenu >= this->items.size()) {
      this->currentMenu = 0;
    }
    this->getCurrentMenu().reset();
  }
  void prev() {
    this->currentMenu--;
    if (this->currentMenu < 0) {
      this->currentMenu = this->items.size() - 1;
    }
    this->getCurrentMenu().reset();
  }
  void reset() {
    this->currentMenu = 0;
    this->getCurrentMenu().reset();
  }
  void goTo(int menu) {
    this->currentMenu = menu;
    if (this->currentMenu >= this->items.size()) {
      this->currentMenu = 0;
    }
    this->getCurrentMenu().reset();
  }
  void open() {
    if (this->open) {
      return;
    }
    this->open = true;
    xTaskCreate(this->taskWrapper, "taskMenue", 10000, NULL, 1, &this->taskhandle);
  }
  void close() {
    if (!this->open) {
      return;
    }
    this->open = false;
    vTaskDelete(this->taskhandle);
  }
  bool state() {
    return this->open;
  }
  MenuEntry getCurrentMenu() { return this->items[this->currentMenu]; }
};