// FastLED Warnung ignorieren
#define FASTLED_INTERNAL

#include "menu/entry.h"
#include "menu/manager.h"
#include "menu/page.h"
#include "led.h"
#include "relais.h"
#include "secrets.h"
#include "state.h"
#include <Arduino.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>
#include <time.h>
#include <algorithm>

#define ONE_WIRE_BUS 4
#define STATUS_LED_RED 18
#define STATUS_LED_GREEN 19
#define STATUS_LED_BLUE 14
#define BUTTON_L 15
#define BUTTON_OK 2
#define BUTTON_R 0
#define FLOAT_SENSOR 36

Relais relais_1(12);
Relais relais_2(27);
Relais relais_3(26);
Relais relais_4(25);
Relais relais_5(33);
Relais relais_6(32);

MenuEntry mainMenu({MenuPage::temp, MenuPage::flow, MenuPage::sensor, MenuPage::function,
                    MenuPage::relayStatus, MenuPage::test});
MenuEntry tempMenu({MenuPage::temp1, MenuPage::temp2});
MenuEntry flowMenu({MenuPage::waterTotal, MenuPage::sensorStatus});
MenuEntry sensorMenu({MenuPage::overflowSensor, MenuPage::sensor5, MenuPage::sensor6});
MenuEntry functionMenu({MenuPage::disinfection, MenuPage::flushMembrane, MenuPage::flushSystem,
                        MenuPage::fillContainer, MenuPage::factoryReset});
MenuEntry relayMenu({MenuPage::relay1, MenuPage::relay2, MenuPage::relay3, MenuPage::relay4, MenuPage::relay5,
                     MenuPage::relay6});
MenuEntry testMenu({MenuPage::ledRingTest});
MenuEntry hiddenMenu({MenuPage::resetConfirm, MenuPage::resetSuccess});

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

LiquidCrystal_I2C lcd(0x27, 16, 2);

State state;

MenuManager menuManager({mainMenu, tempMenu, flowMenu, sensorMenu, functionMenu,
                         relayMenu, testMenu, hiddenMenu},
                        lcd, state);

void IRAM_ATTR handleButtonPressOK() { state.okPressed = true; }
void IRAM_ATTR handleButtonPressL() { state.lPressed = true; }
void IRAM_ATTR handleButtonPressR() { state.rPressed = true; }
void IRAM_ATTR handleFlowImpulse() { state.flowImpulseCount++; }

const char *ntpServer = "pool.ntp.org";
const long gmtOffsetSec = 3600;
const int daylightOffsetSec = 3600;

struct tm timeinfo;

// Funktionsdeklarationen
void getTime();
void taskScheduleManager(void *);

void setup() {

  Serial.begin(9600);

  pinMode(34, INPUT);
  pinMode(STATUS_LED_RED, OUTPUT);
  pinMode(STATUS_LED_GREEN, OUTPUT);
  pinMode(STATUS_LED_BLUE, OUTPUT);

  pinMode(BUTTON_L, INPUT_PULLUP);
  pinMode(BUTTON_OK, INPUT_PULLUP);
  pinMode(BUTTON_R, INPUT_PULLUP);

  pinMode(FLOAT_SENSOR, INPUT);

  // Config aus NVS Speicher auslesen
  if (!state.preferences.begin("Config", true)) {
    Serial.println("Failed to initialize NVS");
    // return;
  }

  state.intervallFlushSystem = state.preferences.getInt("iFlushSystem", 0);
  state.intervallFlushMembrane = state.preferences.getInt("iFlushMembrane", 0);
  state.preferences.end();

  attachInterrupt(digitalPinToInterrupt(BUTTON_OK), handleButtonPressOK,
                  FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_L), handleButtonPressL, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_R), handleButtonPressR, FALLING);
  attachInterrupt(digitalPinToInterrupt(34), handleFlowImpulse, FALLING);

  Serial.println("Starting WiFi...");
  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

  // Init
  configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);

  // Init Temp Sensors
  sensors.begin();

  // Init Display
  lcd.init();
  lcd.backlight();

  // Init LED-Ring
  setupLeds();

  // Get Time from NTP Server
  getTime();

  // Create Threads
  xTaskCreate(ledTask, "taskOne", 10000, &state.ledState, 1, NULL);
  xTaskCreate(taskScheduleManager, "taskScheduleManager", 10000, NULL, 1, NULL);

  state.shortStatus = "OK";
}

void getTime() {

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
}

void loop() {

  if (state.okPressed) {
    Serial.println("DEBUG: OK gedrückt!");
  }

  // printMemoryUsage();
  getTime();
  state.hourOfDay = timeinfo.tm_hour;

  if (state.okPressed && !menuManager.openState()) {
    menuManager.open();
    state.okPressed = false;
  }

  state.flowLiters = round((state.flowImpulseCount * 0.00052) * 100.0) / 100.0;

  if (!menuManager.openState()) {
    lcd.setCursor(0, 0);
    lcd.print("Status: " + state.shortStatus);
    lcd.setCursor(0, 1);
    lcd.print("Wasser: " + String(state.flowLiters) + "L");
    if (state.shortStatus.length() < 3) {
      lcd.setCursor(11, 0);
      lcd.print(&timeinfo, "%R");
    }
  }

  sensors.requestTemperatures();
  state.temp1 = sensors.getTempCByIndex(0);
  state.temp2 = sensors.getTempCByIndex(1);

  delay(500);
}

void taskScheduleManager(void *parameter) {
  unsigned long startMillisFlush;
  unsigned long currentMillisFlush;
  bool startup = true;

  while (true) {

    if (startup) {
      relais_1.turnOn(); // Frischwasserzulauf auf
      delay(500);
      relais_2.turnOn();  // Abwasserventil auf
      delay(2000);        // 300000 = 5m
      relais_3.turnOn();  // Membran-Bypass auf
      delay(2000);        // 300000 = 5m
      relais_3.turnOff(); // Membran-Bypass zu
      relais_2.turnOff(); // Abwasserventil zu
      startup = false;
      startMillisFlush = millis();
    }

    // Alle x Stunden das System spülen
    currentMillisFlush = millis();
    if (currentMillisFlush - startMillisFlush >=
        (state.intervallFlushSystem * 3600000)) {
      relais_2.turnOn(); // Abwasserventil auf
      delay(300000);
      relais_2.turnOff(); // Abwasserventil zu
      startMillisFlush = millis();
    }

    if (state.fillContainer) {
      relais_4.turnOn();
      while (true) {
        delay(500);
        if (digitalRead(FLOAT_SENSOR)) {
          relais_4.turnOff();
          state.fillContainer = false;
          break;
        }
      }
    }

    if (state.flushMembrane) {
      state.shortStatus = "FLUSH M";
      lcd.clear();
      menuManager.close();
      relais_2.turnOn();  // Abwasserventil auf
      relais_3.turnOn();  // Membran-Bypass auf
      delay(5000);        // 300000 = 5m
      relais_3.turnOff(); // Membran-Bypass zu
      relais_2.turnOff(); // Abwasserventil zu
      state.flushMembrane = false;
      state.shortStatus = "OK";
      lcd.clear();
    }

    if (state.flushSystem) {
      state.shortStatus = "FLUSH S";
      lcd.clear();
      menuManager.close();
      relais_2.turnOn();  // Abwasserventil auf
      delay(5000);        // 300000 = 5m
      relais_2.turnOff(); // Abwasserventil zu
      state.flushSystem = false;
      state.shortStatus = "OK";
      lcd.clear();
    }

    delay(500);
  }
}
