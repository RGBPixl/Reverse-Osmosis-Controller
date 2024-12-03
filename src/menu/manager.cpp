#include "manager.h"
#include "../state.h"
#include "entry.h"
#include "page.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

void MenuManager::display() {
  switch (this->getCurrentMenu().getCurrentPage()) {
  case MenuPage::temp:
    lcd.setCursor(0, 0);
    lcd.print("Temperaturen");
    lcd.setCursor(14, 1);
    lcd.print("->");
    break;

  case MenuPage::flow:
    lcd.setCursor(0, 0);
    lcd.print("Durchfluss");
    lcd.setCursor(14, 1);
    lcd.print("->");
    break;

  case MenuPage::sensor:
    lcd.setCursor(0, 0);
    lcd.print("Sonst. Sensoren");
    lcd.setCursor(14, 1);
    lcd.print("->");
    break;

  case MenuPage::function:
    lcd.setCursor(0, 0);
    lcd.print("Funktionen");
    lcd.setCursor(14, 1);
    lcd.print("->");
    break;

  case MenuPage::relayStatus:
    lcd.setCursor(0, 0);
    lcd.print("Status Relais");
    lcd.setCursor(14, 1);
    lcd.print("->");
    break;

  case MenuPage::test:
    lcd.setCursor(0, 0);
    lcd.print("Test");
    lcd.setCursor(14, 1);
    lcd.print("->");
    break;

  case MenuPage::temp1:
    lcd.setCursor(0, 0);
    lcd.print("Vor Filter Temp");
    lcd.setCursor(0, 1);
    lcd.printf("%.2f %sC", state.temp1, "\xDF");
    break;

  case MenuPage::temp2:
    lcd.setCursor(0, 0);
    lcd.print("Nach Filter Temp");
    lcd.setCursor(0, 1);
    lcd.printf("%.2f %sC", state.temp2, "\xDF");
    break;

  case MenuPage::waterTotal:
    lcd.setCursor(0, 0);
    lcd.print("Wasser Total:");
    lcd.setCursor(0, 1);
    lcd.print("9999 L");
    break;

  case MenuPage::sensorStatus:
    lcd.setCursor(0, 0);
    lcd.print("Flow Sensor RAW:");
    lcd.setCursor(0, 1);
    lcd.print("23 IMP/M");
    break;

  case MenuPage::overflowSensor:
    lcd.setCursor(0, 0);
    lcd.print("Overflow Sensor");
    lcd.setCursor(0, 1);
    lcd.print("offen");
    break;

  case MenuPage::sensor5:
    lcd.setCursor(0, 0);
    lcd.print("Dummy Sensor 1");
    lcd.setCursor(0, 1);
    lcd.print("offen");
    break;

  case MenuPage::sensor6:
    lcd.setCursor(0, 0);
    lcd.print("Dummy Sensor 2");
    lcd.setCursor(0, 1);
    lcd.print("offen");
    break;

  case MenuPage::disinfection:
    lcd.setCursor(0, 0);
    lcd.print("Desinfektion");
    lcd.setCursor(0, 1);
    lcd.print("--> Starten <--");
    break;

  case MenuPage::flushMembrane:
    lcd.setCursor(0, 0);
    lcd.printf("Membran Sp%slen", "\xF5");
    lcd.setCursor(0, 1);
    lcd.print("--> Starten <--");
    break;

  case MenuPage::flushSystem:
    lcd.setCursor(0, 0);
    lcd.printf("System Sp%slen", "\xF5");
    lcd.setCursor(0, 1);
    lcd.print("--> Starten <--");
    break;

  case MenuPage::fillContainer:
    lcd.setCursor(0, 0);
    lcd.printf("Kanister f%sllen", "\xF5");
    lcd.setCursor(0, 1);
    lcd.print("--> Starten <--");
    break;

  case MenuPage::relay1:
    lcd.setCursor(0, 0);
    lcd.print("Relais Zulauf");
    lcd.setCursor(0, 1);
    lcd.print("offen");
    break;

  case MenuPage::relay2:
    lcd.setCursor(0, 0);
    lcd.print("Relais Membran");
    lcd.setCursor(0, 1);
    lcd.print("offen");
    break;

  case MenuPage::relay3:
    lcd.setCursor(0, 0);
    lcd.print("Relais Abfluss");
    lcd.setCursor(0, 1);
    lcd.print("offen");
    break;

  case MenuPage::relay4:
    lcd.setCursor(0, 0);
    lcd.print("Relais Kanister");
    lcd.setCursor(0, 1);
    lcd.print("offen");
    break;

  case MenuPage::relay5:
    lcd.setCursor(0, 0);
    lcd.print("Dummy R5");
    lcd.setCursor(0, 1);
    lcd.print("offen");
    break;

  case MenuPage::relay6:
    lcd.setCursor(0, 0);
    lcd.print("Dummy R6");
    lcd.setCursor(0, 1);
    lcd.print("offen");
    break;

  case MenuPage::ledRingTest:
    lcd.setCursor(0, 0);
    lcd.print("Test LED Ring");
    lcd.setCursor(0, 1);
    lcd.printf("Status: %d", state.currentLedTest);
    break;

  case MenuPage::factoryReset:
    lcd.setCursor(0, 0);
    lcd.print("Factory Reset");
    lcd.setCursor(0, 1);
    lcd.print("---> RESET <---");
    break;

  case MenuPage::resetConfirm:
    lcd.setCursor(0, 0);
    lcd.print("RESET");
    lcd.setCursor(0, 1);
    lcd.print("bestaetigen");
    break;

  case MenuPage::resetSuccess:
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RESET");
    lcd.setCursor(0, 1);
    lcd.print("erfolgreich...");
    break;
  
  default:
    lcd.setCursor(0, 0);
    lcd.print("unknown Page");
    break;
  }
}

void MenuManager::handleOk() {
  switch (this->getCurrentMenu().getCurrentPage()) {
  case MenuPage::ledRingTest:
    this->state.currentLedTest++;
    if (this->state.currentLedTest > 6) {
      this->state.currentLedTest = 0;
    }
    this->state.ledState = (LedState)this->state.currentLedTest;
    break;
  case MenuPage::fillContainer:
    this->state.fillContainer = true;
    break;
  case MenuPage::flushMembrane:
    this->state.flushMembrane = true;
    break;
  case MenuPage::flushSystem:
    this->state.flushSystem = true;
    break;
  case MenuPage::factoryReset:
  case MenuPage::resetConfirm:
    this->state.currentResetState++;
    switch (this->state.currentResetState) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("ERROR");
      delay(2000);
      break;
    case 1:
      this->goTo(5); // 5 = relayMenu ?
      break;
    case 2:
      if (!this->state.preferences.begin("Config", false)) {
        Serial.println("Failed to initialize NVS");
        return;
      }
      this->state.preferences.putInt("iFlushSystem", 8);
      this->state.intervallFlushSystem = 8;
      this->state.preferences.putInt("iFlushMembrane", 24);
      this->state.intervallFlushMembrane = 24;
      this->state.preferences.end();
      this->state.currentResetState = 0;
      this->goTo(7); // 7 = hiddenMenu ?
      this->getCurrentMenu().goTo(1); // 7->1 = resetSuccess 
      break;
    }
    break;
  }
  if (this->currentMenu == 0) {
    this->next();
  }
}

void MenuManager::task() {
  this->startMillisIdle = millis();
  this->reset();
  lcd.clear();
  while (true) {
    if (this->state.rPressed) {
      this->startMillisIdle = millis();
      this->getCurrentMenu().next();
      lcd.clear();
      this->state.rPressed = false;
    }
    if (this->state.lPressed) {
      this->startMillisIdle = millis();
      this->getCurrentMenu().reset();
      lcd.clear();
      this->state.lPressed = false;
    }
    if (this->state.okPressed) {
      this->startMillisIdle = millis();
      this->handleOk();
      lcd.clear();
      this->state.okPressed = false;
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
  this->state.currentResetState = 0;
  this->isOpen = false;
  vTaskDelete(this->taskhandle);
}
MenuManager::MenuManager(std::initializer_list<MenuEntry> menus,
                         LiquidCrystal_I2C &lcd, State &state)
    : lcd(lcd), state(state) {
  for (auto menu : menus) {
    this->items.push_back(menu);
  }
  this->currentMenu = 0;
  this->startMillisIdle = millis();
  this->currentMillisIdle = millis();
  this->isOpen = false;
}

bool MenuManager::next() {
  this->currentMenu++;
  if (this->currentMenu >= this->items.size()) {
    this->currentMenu = 0;
    return false;
  }
  this->getCurrentMenu().reset();
  return true;
}

bool MenuManager::prev() {
  if (this->currentMenu <= 0) {
    this->currentMenu = this->items.size() - 1;
    return false;
  }
  this->currentMenu--;
  this->getCurrentMenu().reset();
  return true;
}

// goTo first element of MenuPage of current MenuEntry
void MenuManager::reset() {
  this->goTo(0);
}

// go to specified MenuPage of current MenuEntry
void MenuManager::goTo(int menu) {
  this->currentMenu = menu < this->items.size() ? menu : 0;
  this->getCurrentMenu().reset();
}

void MenuManager::open() {
  if (this->isOpen) {
    return;
  }
  this->isOpen = true;
  // closure [capture grouo](function args) {code}
  xTaskCreate( 
    &*[](void *parameter) {
        auto self = static_cast<MenuManager *>(parameter);
        self->task();
      },
      "taskMenue", 10000, this, 1, &this->taskhandle
    );
}

void MenuManager::close() {
  if (!this->isOpen) {
    return;
  }
  this->isOpen = false;
  vTaskDelete(this->taskhandle);
}