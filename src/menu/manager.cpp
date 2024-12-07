#include "manager.h"
#include "../state.h"
#include "../global_vars.h"
#include "page.h"
#include "error_handling.h"
#include "global_vars.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

MenuManager::MenuManager(std::initializer_list<MenuEntry> menus)
    : currentMenu(0), startMillisIdle(millis()), currentMillisIdle(millis()), isOpen(false) {
  for (MenuEntry menu : menus)
    this->entries.push_back(menu);

  if (&lcd == nullptr || &state == nullptr) {
    error_state = ErrorVars;
  }
}

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
    lcd.printf("%.2f %sC", state->temp1, "\xDF");
    break;

  case MenuPage::temp2:
    lcd.setCursor(0, 0);
    lcd.print("Nach Filter Temp");
    lcd.setCursor(0, 1);
    lcd.printf("%.2f %sC", state->temp2, "\xDF");
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
    lcd.printf("Status: %d", state->currentLedTest);
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
    state->currentLedTest++;
    if (state->currentLedTest > 6) {
      state->currentLedTest = 0;
    }
    state->ledState = (LedState)state->currentLedTest;
    break;
  case MenuPage::fillContainer:
    state->fillContainer = true;
    break;
  case MenuPage::flushMembrane:
    state->flushMembrane = true;
    break;
  case MenuPage::flushSystem:
    state->flushSystem = true;
    break;
  case MenuPage::factoryReset:
  case MenuPage::resetConfirm:
    state->currentResetState++;
    switch (state->currentResetState) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("ERROR");
      delay(2000);
      break;
    case 1:
      this->goTo(5); // 5 = relayMenu ?
      break;
    case 2:
      if (!state->preferences.begin("Config", false)) {
        Serial.println("Failed to initialize NVS");
        return;
      }
      state->preferences.putInt("iFlushSystem", 8);
      state->intervallFlushSystem = 8;
      state->preferences.putInt("iFlushMembrane", 24);
      state->intervallFlushMembrane = 24;
      state->preferences.end();
      state->currentResetState = 0;
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
    if (state->rPressed) {
      this->startMillisIdle = millis();
      this->getCurrentMenu().next();
      lcd.clear();
      state->rPressed = false;
    }
    if (state->lPressed) {
      this->startMillisIdle = millis();
      this->getCurrentMenu().reset();
      lcd.clear();
      state->lPressed = false;
    }
    if (state->okPressed) {
      this->startMillisIdle = millis();
      this->handleOk();
      lcd.clear();
      state->okPressed = false;
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
  state->currentResetState = 0;
  this->isOpen = false;
  vTaskDelete(this->taskhandle);
}

bool MenuManager::next() {
  this->currentMenu++;
  if (this->currentMenu >= this->entries.size()) {
    this->currentMenu = 0;
    return false;
  }
  this->getCurrentMenu().reset();
  return true;
}

bool MenuManager::prev() {
  if (this->currentMenu <= 0) {
    this->currentMenu = this->entries.size() - 1;
    return false;
  }
  this->currentMenu--;
  this->getCurrentMenu().reset();
  return true;
}

// go to specified MenuPage of current MenuEntry
void MenuManager::goTo(int menu) {
  this->currentMenu = menu < this->entries.size() ? menu : 0;
  this->getCurrentMenu().reset();
}

void MenuManager::open() {
  if (this->isOpen) 
    return;
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
  if (!this->isOpen) 
    return;
  this->isOpen = false;
  vTaskDelete(this->taskhandle);
}