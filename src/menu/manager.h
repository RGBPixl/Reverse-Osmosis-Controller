#include <ArduinoSTL.h>

class MenuManager {
private:
  std::vector<MenuEntry> items;
  int currentMenu;
  unsigned long startMillisIdle;
  unsigned long currentMillisIdle;
  bool open;
  TaskHandle_t taskhandle;

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
      lcd.print(String(temp1) + " \xDF"
                                "C");
      break;

    case pageTemp2:
      lcd.setCursor(0, 0);
      lcd.print("Nach Filter Temp");
      lcd.setCursor(0, 1);
      lcd.print(String(temp2) + " \xDF"
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
      lcd.print("Status: " + String(currentLedTest));
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
      currentLedTest++;
      if (currentLedTest > 6) {
        currentLedTest = 0;
      }
      curState = (LedState)currentLedTest;
      break;
    case pageFillContainer:
      fillContainer = true;
      break;
    case pageFlushMembrane:
      flushMembrane = true;
      break;
    case pageFlushSystem:
      flushSystem = true;
      break;
    case pageFactoryReset:
    case pageResetConfirm:
      currentResetState++;
      switch (currentResetState) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print("ERROR");
        delay(2000);
        break;
      case 1:
        this->goTo(5);
        break;
      case 2:
        if (!preferences.begin("Config", false)) {
          Serial.println("Failed to initialize NVS");
          return;
        }
        preferences.putInt("iFlushSystem", 8);
        intervallFlushSystem = 8;
        preferences.putInt("iFlushMembrane", 24);
        intervallFlushMembrane = 24;
        preferences.end();
        currentResetState = 0;
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

public:
  MenuManager(std::initializer_list<MenuEntry> menus) : items(menus) {
    this->currentMenu = 0;
    this->startMillisIdle = millis();
    this->currentMillisIdle = millis();
    this->open = false;
  }
  void task(void *parameter) {
    this->startMillisIdle = millis();
    this->reset();
    lcd.clear();
    while (true) {
      if (rPressed) {
        this->startMillisIdle = millis();
        this->getCurrentMenu().next();
        lcd.clear();
        rPressed = false;
      }
      if (lPressed) {
        this->startMillisIdle = millis();
        this->getCurrentMenu().reset();
        lcd.clear();
        lPressed = false;
      }
      if (okPressed) {
        this->startMillisIdle = millis();
        this->handleOk();
        lcd.clear();
        okPressed = false;
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
    currentResetState = 0;
    this->open = false;
    vTaskDelete(this->taskhandle);
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
    xTaskCreate(this->task, "taskMenue", 10000, NULL, 1, &this->taskhandle);
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