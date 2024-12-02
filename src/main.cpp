// FastLED Warnung ignorieren
#define FASTLED_INTERNAL

#include "menu/entry.h"
#include "menu/page.h"
#include "menu/manager.h"
#include "relais.h"
#include "secrets.h"
#include <Arduino.h>
#include <DallasTemperature.h>
#include <FastLED.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include <math.h>

#define ONE_WIRE_BUS 4
#define LED_PIN 23
#define LED_COUNT 8
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

enum LedState {
  staticRed,
  spinningRed,
  staticRainbow,
  staticGreen,
  staticBlue,
  spinningBlue,
  spinningRainbow
};

MenuEntry mainMenu({pageTemp, pageFlow, pageSensor, pageFunction, pageRelayStatus, pageTest});
MenuEntry tempMenu({pageTemp1, pageTemp2});
MenuEntry flowMenu({pageWaterTotal, pageSensorStatus});
MenuEntry sensorMenu({pageOverflowSensor, pageSensor5, pageSensor6});
MenuEntry functionMenu({pageDisinfection, pageFlushMembrane, pageFlushSystem,
                   pageFillContainer, pageFactoryReset});
MenuEntry relayMenu({pageRelay1, pageRelay2, pageRelay3, pageRelay4, pageRelay5,
                pageRelay6});
MenuEntry testMenu({pageLedRingTest});
MenuEntry hiddenMenu({pageResetConfirm, pageResetSuccess});

MenuManager menuManager({mainMenu, tempMenu, flowMenu, sensorMenu, functionMenu,
                         relayMenu, testMenu, hiddenMenu});

int currentLedTest = 0;
int currentResetState = 0;
String shortStatus = "BOOT"; // max. 8 Zeichen

int flowImpulseCount = 0;
float flowLiters;
bool fillContainer = false;
bool flushMembrane = false;
bool flushSystem = false;

int hourOfDay;

int testState = 0;
double temp1;
double temp2;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

LedState curState = spinningRed;

LiquidCrystal_I2C lcd(0x27, 16, 2);
CRGB leds[LED_COUNT];

volatile bool okPressed = false;
volatile bool lPressed = false;
volatile bool rPressed = false;

void IRAM_ATTR handleButtonPressOK() { okPressed = true; }
void IRAM_ATTR handleButtonPressL() { lPressed = true; }
void IRAM_ATTR handleButtonPressR() { rPressed = true; }
void IRAM_ATTR handleFlowImpulse() { flowImpulseCount++; }

const char *ntpServer = "pool.ntp.org";
const long gmtOffsetSec = 3600;
const int daylightOffsetSec = 3600;

struct tm timeinfo;

Preferences preferences;
int intervallFlushSystem;
int intervallFlushMembrane;

// Funktionsdeklarationen
void getTime();
void taskOne(void *);
void taskScheduleManager(void *);
void taskMenu(void *parameter) {
  menuManager.task(parameter);
}

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
  if (!preferences.begin("Config", true)) {
    Serial.println("Failed to initialize NVS");
    // return;
  }

  intervallFlushSystem = preferences.getInt("iFlushSystem", 0);
  intervallFlushMembrane = preferences.getInt("iFlushMembrane", 0);
  preferences.end();

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
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(30);

  // Get Time from NTP Server
  getTime();

  // Create Threads
  xTaskCreate(taskOne, "taskOne", 10000, &curState, 1, NULL);
  xTaskCreate(taskScheduleManager, "taskScheduleManager", 10000, NULL, 1, NULL);

  shortStatus = "OK";
}

void getTime() {

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
}

void loop() {

  if (okPressed) {
    Serial.println("DEBUG: OK gedrückt!");
  }

  // printMemoryUsage();
  getTime();
  hourOfDay = timeinfo.tm_hour;

  if (okPressed && !menuManager.state()) {
    menuManager.open();
    okPressed = false;
  }

  flowLiters = round((flowImpulseCount * 0.00052) * 100.0) / 100.0;

  if (!menuManager.state()) {
    lcd.setCursor(0, 0);
    lcd.print("Status: " + shortStatus);
    lcd.setCursor(0, 1);
    lcd.print("Wasser: " + String(flowLiters) + "L");
    if (shortStatus.length() < 3) {
      lcd.setCursor(11, 0);
      lcd.print(&timeinfo, "%R");
    }
  }

  sensors.requestTemperatures();
  temp1 = sensors.getTempCByIndex(0);
  temp2 = sensors.getTempCByIndex(1);

  delay(500);
}

void taskOne(void *parameter) {
  LedState *state;
  state = (LedState *)parameter;

  while (true) {

    switch (*state) {

    case staticRed:
      fill_solid(leds, LED_COUNT, CRGB::Red);
      FastLED.show();
      break;

    case spinningRed:
      for (int i = 0; i < LED_COUNT; i++) {
        fadeToBlackBy(leds, LED_COUNT, 100);
        leds[i] = CRGB::Red;
        FastLED.show();
        delay(100);
      }
      break;

    case staticRainbow:
      fill_rainbow(leds, LED_COUNT, 0, 32);
      FastLED.show();
      break;

    case staticGreen:
      fill_solid(leds, LED_COUNT, CRGB::Green);
      FastLED.show();
      break;

    case staticBlue:
      fill_solid(leds, LED_COUNT, CRGB::Blue);
      FastLED.show();
      break;

    case spinningBlue:
      for (int i = 0; i < LED_COUNT; i++) {
        fadeToBlackBy(leds, LED_COUNT, 100);
        leds[i] = CRGB::Blue;
        FastLED.show();
        delay(100);
      }
      break;

    case spinningRainbow:
      for (int i = LED_COUNT - 1; i >= 0; i--) {
        fill_rainbow(leds, LED_COUNT, i * 32, 32);
        FastLED.show();
        delay(100);
      }
      break;
    }

    vTaskDelay(1); // Leichtes Yielding, um den Watchdog nicht zu triggern
  }
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
        (intervallFlushSystem * 3600000)) {
      relais_2.turnOn(); // Abwasserventil auf
      delay(300000);
      relais_2.turnOff(); // Abwasserventil zu
      startMillisFlush = millis();
    }

    if (fillContainer) {
      relais_4.turnOn();
      while (true) {
        delay(500);
        if (digitalRead(FLOAT_SENSOR)) {
          relais_4.turnOff();
          fillContainer = false;
          break;
        }
      }
    }

    if (flushMembrane) {
      shortStatus = "FLUSH M";
      lcd.clear();
      menuManager.close();
      relais_2.turnOn();  // Abwasserventil auf
      relais_3.turnOn();  // Membran-Bypass auf
      delay(5000);        // 300000 = 5m
      relais_3.turnOff(); // Membran-Bypass zu
      relais_2.turnOff(); // Abwasserventil zu
      flushMembrane = false;
      shortStatus = "OK";
      lcd.clear();
    }

    if (flushSystem) {
      shortStatus = "FLUSH S";
      lcd.clear();
      menuManager.close();
      relais_2.turnOn();  // Abwasserventil auf
      delay(5000);        // 300000 = 5m
      relais_2.turnOff(); // Abwasserventil zu
      flushSystem = false;
      shortStatus = "OK";
      lcd.clear();
    }

    delay(500);
  }
}