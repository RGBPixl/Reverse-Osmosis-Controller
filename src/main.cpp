//FastLED Warnung ignorieren
#define FASTLED_INTERNAL

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FastLED.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include "secrets.h"
#include "classes.h"

#define ONE_WIRE_BUS 4
#define LED_PIN 23
#define LED_COUNT 8
#define statusLED_red 18
#define statusLED_green 19
#define statusLED_blue 14
#define button_l 15
#define button_ok 2
#define button_r 0
#define floatSensor 36

Relais relais_1(12);
Relais relais_2(27);
Relais relais_3(26);
Relais relais_4(25);
Relais relais_5(33);
Relais relais_6(32);

enum LedState { staticRed,
                spinningRed,
                staticRainbow,
                staticGreen,
                staticBlue,
                spinningBlue,
                spinningRainbow };

enum MenuePage { pageTemp,
                 pageFlow,
                 pageSensor,
                 pageFunction,
                 pageRelayStatus,
                 pageTest,
                 pageTemp1,
                 pageTemp2,
                 pageWaterTotal,
                 pageSensorStatus,
                 pageOverflowSensor,
                 pageSensor5,
                 pageSensor6,
                 pageDisinfection,
                 pageFlushMembrane,
                 pageFlushSystem,
                 pageFillContainer,
                 pageFactoryReset,
                 pageRelay1,
                 pageRelay2,
                 pageRelay3,
                 pageRelay4,
                 pageRelay5,
                 pageRelay6,
                 pageLedRingTest,
                 pageResetConfirm,
                 pageResetSuccess,
                 nullPage };

MenuePage MenueArray[8][6] = { { pageTemp, pageFlow, pageSensor, pageFunction, pageRelayStatus, pageTest },                              //Hauptmenü
                               { pageTemp1, pageTemp2, nullPage, nullPage, nullPage, nullPage },                                         //Temp Menü
                               { pageWaterTotal, pageSensorStatus, nullPage, nullPage, nullPage, nullPage },                             //Flow Menü
                               { pageOverflowSensor, pageSensor5, pageSensor6, nullPage, nullPage, nullPage },                           //Sensor Menü
                               { pageDisinfection, pageFlushMembrane, pageFlushSystem, pageFillContainer, pageFactoryReset, nullPage },  //Function Menü
                               { pageRelay1, pageRelay2, pageRelay3, pageRelay4, pageRelay5, pageRelay6 },                               //Relay Menü
                               { pageLedRingTest, nullPage, nullPage, nullPage, nullPage, nullPage },                                    //Test Menü
                               { pageResetConfirm, pageResetSuccess, nullPage, nullPage, nullPage, nullPage } };                         //Hidden Menues
int currentMenu;
int currentPage;
int currentLedTest = 0;
int currentResetState = 0;
String shortStatus = "BOOT"; //max. 8 Zeichen

int flowImpulseCount = 0;
float flowLiters;
bool fillContainer = false;
bool flushMembrane = false;
bool flushSystem = false;

int hourOfDay;

int testState = 0;
bool menueOpen = false;
double Temp1;
double Temp2;

unsigned long startMillisIdle;
unsigned long currentMillisIdle;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

LedState curState = spinningRed;
//int curMPage;

LiquidCrystal_I2C lcd(0x27, 16, 2);
CRGB leds[LED_COUNT];

volatile bool okPressed = false;
volatile bool lPressed = false;
volatile bool rPressed = false;

TaskHandle_t TaskHandle_1;

void IRAM_ATTR handleButtonPressOK() {
  okPressed = true;
}
void IRAM_ATTR handleButtonPressL() {
  lPressed = true;
}
void IRAM_ATTR handleButtonPressR() {
  rPressed = true;
}
void IRAM_ATTR handleFlowImpulse() {
  flowImpulseCount++;
}

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

struct tm timeinfo;

Preferences preferences;
int intervallFlushSystem;
int intervallFlushMembrane;

//Funktionsdeklarationen
void getTime();
void taskOne(void *);
void taskScheduleManager(void *);
void taskMenue(void *);


void setup() {

  Serial.begin(9600);

  pinMode(34, INPUT);
  pinMode(statusLED_red, OUTPUT);
  pinMode(statusLED_green, OUTPUT);
  pinMode(statusLED_blue, OUTPUT);

  pinMode(button_l, INPUT_PULLUP);
  pinMode(button_ok, INPUT_PULLUP);
  pinMode(button_r, INPUT_PULLUP);

  pinMode(floatSensor, INPUT);

  //Config aus NVS Speicher auslesen
  if (!preferences.begin("Config", true)) {
    Serial.println("Failed to initialize NVS");
    //return;
  }

  intervallFlushSystem = preferences.getInt("iFlushSystem", 0);
  intervallFlushMembrane = preferences.getInt("iFlushMembrane", 0);
  preferences.end();

  attachInterrupt(digitalPinToInterrupt(button_ok), handleButtonPressOK, FALLING);
  attachInterrupt(digitalPinToInterrupt(button_l), handleButtonPressL, FALLING);
  attachInterrupt(digitalPinToInterrupt(button_r), handleButtonPressR, FALLING);
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
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Init Temp Sensors
  sensors.begin();

  //Init Display
  lcd.init();
  lcd.backlight();

  //Init LED-Ring
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(30);

  //Get Time from NTP Server
  getTime();

  //Create Threads
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

  if (okPressed){
    Serial.println("DEBUG: OK gedrückt!");
  }

  //printMemoryUsage();
  getTime();
  hourOfDay = timeinfo.tm_hour;

  if (okPressed && !menueOpen) {
    menueOpen = true;
    xTaskCreate(taskMenue, "taskMenue", 10000, NULL, 1, &TaskHandle_1);
    okPressed = false;
  }

  flowLiters = round((flowImpulseCount * 0.00052) * 100.0) / 100.0;

  if (!menueOpen) {
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
  Temp1 = sensors.getTempCByIndex(0);
  Temp2 = sensors.getTempCByIndex(1);

  delay(500);
}

void taskMenue(void *parameter) {

  startMillisIdle = millis();
  currentMenu = 0;
  currentPage = 0;
  lcd.clear();

  while(true){
    if(rPressed){
      startMillisIdle = millis();
      currentPage++;
      if (MenueArray[currentMenu][currentPage] == nullPage || currentPage == 6){
        currentPage = 0;
      }
      lcd.clear();
      rPressed = false;
    }

    if(lPressed){
      startMillisIdle = millis();
      currentPage = currentMenu -1;
      if (currentPage == -1){
        lPressed = false;
        break;
      }
      currentMenu = 0;
      currentResetState = 0;
      lcd.clear();
      lPressed = false;
    }

    if(okPressed){
      startMillisIdle = millis();
      lcd.clear();
      okPressed = false;

      if (currentMenu == 6 && currentPage == 0){
        currentLedTest++;
        if (currentLedTest > 6){
          currentLedTest = 0;
        }
        curState = (LedState)currentLedTest;
      }
  
      //Fill Container
      if (currentMenu == 4 && currentPage == 3){
        fillContainer = true;
      }

      //Flush Membrane
      if (currentMenu == 4 && currentPage == 1){
        flushMembrane = true;
      }

      //Flush System
      if (currentMenu == 4 && currentPage == 2){
        flushSystem = true;
      }

      //Factory Reset
      if ((currentMenu == 4 && currentPage == 4) || (currentMenu == 7 && currentPage == 0)){
        currentResetState++;
        switch (currentResetState) {
          
          case 0:
            lcd.setCursor(0, 0);
            lcd.print("ERROR");
            delay(2000);
            break;

          case 1:
            currentMenu = 7;
            currentPage = 0;
            break;

          case 2:

            if (!preferences.begin("Config", false)) {
              Serial.println("Failed to initialize NVS");
            return;
            }
            preferences.putInt("iFlushSystem", 8);     //Default alle 8 Stunden
            intervallFlushSystem = 8;
            preferences.putInt("iFlushMembrane", 24);  //Default alle 24 Stunden
            intervallFlushMembrane = 24;
            preferences.end();
            currentResetState = 0;

            currentMenu = 7;
            currentPage = 1;
            break;
          }
        }
        if (currentMenu == 0){
          currentMenu = currentPage +1;
          currentPage = 0;
        }
      }

    switch((MenuePage)MenueArray[currentMenu][currentPage]){

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
        lcd.print(String(Temp1) + " \xDF" "C");
        break;

      case pageTemp2:
        lcd.setCursor(0, 0);
        lcd.print("Nach Filter Temp");
        lcd.setCursor(0, 1);
        lcd.print(String(Temp2) + " \xDF" "C");
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
        lcd.print("Membran Sp" "\xF5" "len");
        lcd.setCursor(0, 1);
        lcd.print("--> Starten <--");
        break;

      case pageFlushSystem:
        lcd.setCursor(0, 0);
        lcd.print("System Sp" "\xF5" "len");
        lcd.setCursor(0, 1);
        lcd.print("--> Starten <--");
        break;

      case pageFillContainer:
        lcd.setCursor(0, 0);
        lcd.print("Kanister f" "\xF5" "llen");
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

      default:
        lcd.setCursor(0, 0);
        lcd.print("ERROR 404");
        lcd.setCursor(0, 1);
        lcd.print("M:" + String(currentMenu) + " P:" + String(currentPage));
        break; 
    }

    currentMillisIdle = millis();
    if (currentMillisIdle - startMillisIdle >= 10000) {
      break;
    } 
    delay(500);
    vTaskDelay(1);
  }

  lcd.clear();
  currentResetState = 0;
  menueOpen = false;
  vTaskDelete(NULL);
}

void taskOne(void *parameter) {
  LedState *state;
  state = (LedState*)parameter;

  while (true) {
    
    switch (*state) {

      case staticRed:
        fill_solid(leds, LED_COUNT, CRGB::Red);
        FastLED.show();
        break;

      case spinningRed:
        for (int i = 0; i<LED_COUNT; i++){
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
        for (int i = 0; i<LED_COUNT; i++){
          fadeToBlackBy(leds, LED_COUNT, 100);
          leds[i] = CRGB::Blue;
          FastLED.show();
          delay(100);
        }
        break;

     case spinningRainbow:
      for (int i = LED_COUNT-1; i>=0; i--){
        fill_rainbow(leds, LED_COUNT, i*32, 32);
        FastLED.show();
        delay(100);
      }
        break;     	
    }

    vTaskDelay(1);  // Leichtes Yielding, um den Watchdog nicht zu triggern
  }
}


void taskScheduleManager(void *parameter) {
  unsigned long startMillisFlush;
  unsigned long currentMillisFlush;
  bool startup = true;

  while (true) {

    if (startup) {
      relais_1.turnOn();  //Frischwasserzulauf auf
      delay(500);
      relais_2.turnOn();   //Abwasserventil auf
      delay(2000);         //300000 = 5m
      relais_3.turnOn();   //Membran-Bypass auf
      delay(2000);         //300000 = 5m
      relais_3.turnOff();  //Membran-Bypass zu
      relais_2.turnOff();  //Abwasserventil zu
      startup = false;
      startMillisFlush = millis();
    }

    //Alle x Stunden das System spülen
    currentMillisFlush = millis();
    if (currentMillisFlush - startMillisFlush >= (intervallFlushSystem * 3600000)) {
      relais_2.turnOn();  //Abwasserventil auf
      delay(300000);
      relais_2.turnOff();  //Abwasserventil zu
      startMillisFlush = millis();
    }

    if (fillContainer) {
      relais_4.turnOn();
      while (true) {
        delay(500);
        if (digitalRead(floatSensor)) {
          relais_4.turnOff();
          fillContainer = false;
          break;
        }
      }
    }

    if (flushMembrane) {
      shortStatus = "FLUSH M";
      lcd.clear();
      menueOpen = false;
      vTaskDelete(TaskHandle_1);
      relais_2.turnOn();   //Abwasserventil auf
      relais_3.turnOn();   //Membran-Bypass auf
      delay(5000);         //300000 = 5m
      relais_3.turnOff();  //Membran-Bypass zu
      relais_2.turnOff();  //Abwasserventil zu
      flushMembrane = false;
      shortStatus = "OK";
      lcd.clear();
    }

    if (flushSystem) {
      shortStatus = "FLUSH S";
      lcd.clear();
      menueOpen = false;
      vTaskDelete(TaskHandle_1);
      relais_2.turnOn();   //Abwasserventil auf
      delay(5000);         //300000 = 5m
      relais_2.turnOff();  //Abwasserventil zu
      flushSystem = false;
      shortStatus = "OK";
      lcd.clear();
    }

    delay(500);
  }
}