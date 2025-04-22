#include "manager.h"
#include "../state.h"
#include "../global_vars.h"
#include "page.h"
#include "error_handling.h"
#include "global_vars.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>

extern bool mqtt_enabled;

MenuManager::MenuManager(std::initializer_list<MenuEntry> menus)
    : currentMenu(0), startMillisIdle(millis()), currentMillisIdle(millis()), isOpen(false) {
  for(MenuEntry e : menus) {
    entries.push_back(e);
  }
}

void MenuManager::display() {
  switch ( entries[currentMenu].getCurrentPage() ) {
  case MenuPage::temp:
    lcd->setCursor(0, 0);
    lcd->print("Temperaturen");
    lcd->setCursor(14, 1);
    lcd->print("->");
    break;

  case MenuPage::flow:
    lcd->setCursor(0, 0);
    lcd->print("Durchfluss");
    lcd->setCursor(14, 1);
    lcd->print("->");
    break;

  case MenuPage::sensor:
    lcd->setCursor(0, 0);
    lcd->print("Sonst. Sensoren");
    lcd->setCursor(14, 1);
    lcd->print("->");
    break;

  case MenuPage::function:
    lcd->setCursor(0, 0);
    lcd->print("Funktionen");
    lcd->setCursor(14, 1);
    lcd->print("->");
    break;

  case MenuPage::mqttConfig:
    lcd->setCursor(0, 0);
    lcd->print("MQTT Setup");
    lcd->setCursor(0, 1);
    lcd->print("--> Starten <--");
    break;

  case MenuPage::relayStatus:
    lcd->setCursor(0, 0);
    lcd->print("Status Relais");
    lcd->setCursor(14, 1);
    lcd->print("->");
    break;

  case MenuPage::test:
    lcd->setCursor(0, 0);
    lcd->print("Test");
    lcd->setCursor(14, 1);
    lcd->print("->");
    break;

  case MenuPage::temp1:
    lcd->setCursor(0, 0);
    lcd->print("Vor Filter Temp");
    lcd->setCursor(0, 1);
    lcd->printf("%.2f %sC", state->temp1, "\xDF");
    break;

  case MenuPage::temp2:
    lcd->setCursor(0, 0);
    lcd->print("Nach Filter Temp");
    lcd->setCursor(0, 1);
    lcd->printf("%.2f %sC", state->temp2, "\xDF");
    break;

  case MenuPage::waterTotal:
    lcd->setCursor(0, 0);
    lcd->print("Wasser Total:");
    lcd->setCursor(0, 1);
    lcd->print("9999 L");
    break;

  case MenuPage::sensorStatus:
    lcd->setCursor(0, 0);
    lcd->print("Flow Sensor RAW:");
    lcd->setCursor(0, 1);
    lcd->print("23 IMP/M");
    break;

  case MenuPage::overflowSensor:
    lcd->setCursor(0, 0);
    lcd->print("Overflow Sensor");
    lcd->setCursor(0, 1);
    lcd->print("offen");
    break;

  case MenuPage::sensor5:
    lcd->setCursor(0, 0);
    lcd->print("Dummy Sensor 1");
    lcd->setCursor(0, 1);
    lcd->print("offen");
    break;

  case MenuPage::sensor6:
    lcd->setCursor(0, 0);
    lcd->print("Dummy Sensor 2");
    lcd->setCursor(0, 1);
    lcd->print("offen");
    break;

  case MenuPage::disinfection:
    lcd->setCursor(0, 0);
    lcd->print("Desinfektion");
    lcd->setCursor(0, 1);
    lcd->print("--> Starten <--");
    break;

  case MenuPage::flushMembrane:
    lcd->setCursor(0, 0);
    lcd->printf("Membran Sp%slen\n", "\xF5");
    lcd->setCursor(0, 1);
    lcd->print("--> Starten <--");
    break;

  case MenuPage::flushSystem:
    lcd->setCursor(0, 0);
    lcd->printf("System Sp%slen\n", "\xF5");
    lcd->setCursor(0, 1);
    lcd->print("--> Starten <--");
    break;

  case MenuPage::fillContainer:
    lcd->setCursor(0, 0);
    lcd->printf("Kanister f%sllen\n", "\xF5");
    lcd->setCursor(0, 1);
    lcd->print("--> Starten <--");
    break;

  case MenuPage::relay1:
    lcd->setCursor(0, 0);
    lcd->print("Relais Zulauf");
    lcd->setCursor(0, 1);
    lcd->print("offen");
    break;

  case MenuPage::relay2:
    lcd->setCursor(0, 0);
    lcd->print("Relais Membran");
    lcd->setCursor(0, 1);
    lcd->print("offen");
    break;

  case MenuPage::relay3:
    lcd->setCursor(0, 0);
    lcd->print("Relais Abfluss");
    lcd->setCursor(0, 1);
    lcd->print("offen");
    break;

  case MenuPage::relay4:
    lcd->setCursor(0, 0);
    lcd->print("Relais Kanister");
    lcd->setCursor(0, 1);
    lcd->print("offen");
    break;

  case MenuPage::relay5:
    lcd->setCursor(0, 0);
    lcd->print("Dummy R5");
    lcd->setCursor(0, 1);
    lcd->print("offen");
    break;

  case MenuPage::relay6:
    lcd->setCursor(0, 0);
    lcd->print("Dummy R6");
    lcd->setCursor(0, 1);
    lcd->print("offen");
    break;

  case MenuPage::ledRingTest:
    lcd->setCursor(0, 0);
    lcd->print("Test LED Ring");
    lcd->setCursor(0, 1);
    lcd->printf("Status: %d\n", state->currentLedTest);
    break;

  case MenuPage::factoryReset:
    lcd->setCursor(0, 0);
    lcd->print("Factory Reset");
    lcd->setCursor(0, 1);
    lcd->print("---> RESET <---");
    break;

  case MenuPage::resetConfirm:
    lcd->setCursor(0, 0);
    lcd->print("RESET");
    lcd->setCursor(0, 1);
    lcd->print("bestaetigen");
    break;

  case MenuPage::resetSuccess:
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print("RESET");
    lcd->setCursor(0, 1);
    lcd->print("erfolgreich...");
    break;
  
  default:
    lcd->setCursor(0, 0);
    lcd->print("unknown Page");
    break;
  }
  // Serial.printf("%d\r\n", getCurrentMenu().getCurrentPageInt());
}

void MenuManager::handleOk() {
  switch (getCurrentMenu().getCurrentPage()) {
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
          lcd->setCursor(0, 0);
          lcd->print("ERROR");
          delay(2000);
          break;
        case 1:
          this->setMenu(5); // 5 = relayMenu ?
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
          setMenu(7); // 7 = hiddenMenu ?
          getCurrentMenu().setPage(1); // 7->1 = resetSuccess 
          break;
      }
      break;

    case MenuPage::mqttConfig:
      lcd->clear();
      lcd->setCursor(0, 0);
      lcd->print("Webpanel:");
      lcd->setCursor(0, 1);
      lcd->print(WiFi.localIP().toString().c_str());
      delay(3000);
      close();
      break;
    

  }

  if (currentMenu == 0) {
    setMenu(getCurrentMenu().getCurrentPageInt() + 1);
  }
}


void MenuManager::task() {
  startMillisIdle = millis();
  reset();
  lcd->clear();
  while (true) {
    if (state->rPressed) {
      // Serial.print("rPressed\r\n");
      startMillisIdle = millis();
      getCurrentMenu().next();
      lcd->clear();
      state->rPressed = false;
    }
    if (state->lPressed) {
      // Serial.print("lPressed\r\n");
      startMillisIdle = millis();
      // getCurrentMenu().prev();
      getCurrentMenu().reset();
      setMenu(0);
      lcd->clear();
      state->lPressed = false;
    }
    if (state->okPressed) {
      // Serial.print("okPressed\r\n");
      startMillisIdle = millis();
      handleOk();
      lcd->clear();
      state->okPressed = false;
    }
    display();
    currentMillisIdle = millis();
    if (currentMillisIdle - startMillisIdle >= 10000) {
      break;
    }
    delay(500);
    vTaskDelay(1);
  }
  lcd->clear();
  state->currentResetState = 0;
  isOpen = false;
  vTaskDelete(taskhandle);
}

void MenuManager::next() {
  currentMenu = currentMenu < entries.size() - 1 ? currentMenu+1 : 0;
}

void MenuManager::prev() {
  currentMenu = currentMenu > 0 ? currentMenu-1 : entries.size()-1;
}

// go to specified MenuPage of current MenuEntry
void MenuManager::setMenu(int menu) {
  currentMenu = menu < entries.size() ? menu : 0;
  getCurrentMenu().reset();
}

void MenuManager::open() {
  if (this->isOpen) 
    return;
  isOpen = true;
  // closure [capture grouo](function args) {code}
  xTaskCreate( 
    &*[](void *parameter) {
        auto self = static_cast<MenuManager *>(parameter);
        self->task();
      },
      "taskMenue", 10000, this, 1, &taskhandle
    );
}

void MenuManager::close() {
  if (!this->isOpen) 
    return;
  this->isOpen = false;
  vTaskDelete(taskhandle);
}