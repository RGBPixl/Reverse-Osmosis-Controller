#include "error_handling.h"
#include "led.h"
#include "menu/entry.h"
#include "menu/manager.h"
#include "menu/page.h"
#include "relais.h"
#include "state.h"
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <math.h>
#include <time.h>
#include <vector>
#include "WiFiIcons.h"

#define ONE_WIRE_BUS 4
#define STATUS_LED_RED 18
#define STATUS_LED_GREEN 19
#define STATUS_LED_BLUE 14
#define BUTTON_OK 2
#define BUTTON_L 15
#define BUTTON_R 0
#define FLOW_SENSOR 34
#define FLOAT_SENSOR 36

#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 3600
#define DAYLIGHT_OFFSET_SEC 3600

Relais *relais[6]; 

DallasTemperature *sensors; 
LiquidCrystal_I2C *lcd;

State *state;
MenuManager *menuManager;
ErrorType error_state = ErrorOK;

struct tm timeinfo;

// Funktionsdeklarationen
void IRAM_ATTR handleButtonPressOK(){ state->okPressed = true; }
void IRAM_ATTR handleButtonPressL() { state->lPressed = true; }
void IRAM_ATTR handleButtonPressR() { state->rPressed = true; }
void IRAM_ATTR handleFlowImpulse()  { state->flowImpulseCount++; }

void taskScheduleManager(void *);
void setup();
void loop();

bool flushTodayDone[2] = { false, false };

int main(int argc, char **argv){
  setup();
  Serial.print("Setup done\n");
  do {
    loop();
  } while(error_state == ErrorOK);
    

  switch (error_state) {
  case ErrorOK:
    break;
  case ErrorWIFI:
    Serial.print("keine WLAN Verbindung");
    break;
  case ErrorVars:
    Serial.print("Fehler bei der Variablendeklaration");
    break;
  case ErrorTime:
    Serial.print("Fehler bei der Zeiteinstellung");
    break;
  default:
    break;
  }

  delete(state);
  delete(lcd);
  delete(menuManager);
  delete(sensors);
  for(Relais *r : relais) {
    delete(r);
  }
  return 0;
}

void setup() {
  
  Serial.begin(9600);

  pinMode(FLOW_SENSOR, INPUT);
  pinMode(STATUS_LED_RED, OUTPUT);
  pinMode(STATUS_LED_GREEN, OUTPUT);
  pinMode(STATUS_LED_BLUE, OUTPUT);

  pinMode(BUTTON_L, INPUT_PULLUP);
  pinMode(BUTTON_OK, INPUT_PULLUP);
  pinMode(BUTTON_R, INPUT_PULLUP);

  pinMode(FLOAT_SENSOR, INPUT);

  relais[0] = new Relais(12);
  relais[1] = new Relais(27);
  relais[2] = new Relais(26);
  relais[3] = new Relais(25);
  relais[4] = new Relais(33);
  relais[5] = new Relais(32);

  OneWire oneWire(ONE_WIRE_BUS);
  sensors = new DallasTemperature(&oneWire);

  state = new State();
  // Config aus NVS Speicher auslesen
  if (!state->preferences.begin("Config", true)) {
    Serial.println("Failed to initialize NVS");
  }

  state->intervallFlushSystem = state->preferences.getInt("iFlushSystem", 0);
  state->intervallFlushMembrane = state->preferences.getInt("iFlushMembrane", 0);
  state->flushTime1Hour = state->preferences.getInt("flushTime1Hour", 0);
  state->flushTime1Minute = state->preferences.getInt("flushTime1Minute", 0);
  state->flushTime2Hour = state->preferences.getInt("flushTime2Hour", 0);
  state->flushTime2Minute = state->preferences.getInt("flushTime2Minute", 0);
  state->preferences.end();

  attachInterrupt(digitalPinToInterrupt(BUTTON_OK), handleButtonPressOK, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_L), handleButtonPressL, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_R), handleButtonPressR, FALLING);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR), handleFlowImpulse, FALLING);

  //WiFi Connection
  Serial.println("Starting WiFiManager...");

  // WiFiManager-Instanz
  WiFiManager wm;

  // Optional: Timeout für das Konfigurationsportal (z. B. 180 Sekunden)
  wm.setConfigPortalTimeout(180);

  // Versuch, mit gespeicherten Daten zu verbinden – sonst AP starten
  if (!wm.autoConnect("Reverse-Osmosis-Controller_AP")) {
    Serial.println("Failed to connect and hit timeout");
    error_state = ErrorWIFI;
    return;
  } else {
    error_state = ErrorOK;
  }

  Serial.println("WiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Init
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  // Init Temp Sensors
  sensors->begin();

  lcd = new LiquidCrystal_I2C(0x27, 16, 2);
  // Init Display
  lcd->init();
  lcd->backlight();

  lcd->createChar(0, wifiLevel0);
  lcd->createChar(1, wifiLevel1);
  lcd->createChar(2, wifiLevel2);
  lcd->createChar(3, wifiLevel3);
  lcd->createChar(4, wifiDisconnected);
  

  // Init LED-Ring
  setupLeds();
  // Get Time from NTP Server
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  //  exit(-1);
  }
  //Zeit in Konsole ausgeben
  char timeStringBuff[64]; // Buffer für Zeit-String
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  Serial.println("Aktuelle Zeit: " + String(timeStringBuff));

  MenuEntry mainMenu({MenuPage::temp, MenuPage::flow, MenuPage::sensor, 
                    MenuPage::function, MenuPage::relayStatus, MenuPage::test});
  MenuEntry tempMenu({MenuPage::temp1, MenuPage::temp2});
  MenuEntry flowMenu({MenuPage::waterTotal, MenuPage::sensorStatus});
  MenuEntry sensorMenu({MenuPage::overflowSensor, MenuPage::sensor5,
                      MenuPage::sensor6});
  MenuEntry functionMenu({MenuPage::disinfection, MenuPage::flushMembrane,
                        MenuPage::flushSystem, MenuPage::fillContainer,
                        MenuPage::factoryReset});
  MenuEntry relayMenu({MenuPage::relay1, MenuPage::relay2, MenuPage::relay3,
                     MenuPage::relay4, MenuPage::relay5, MenuPage::relay6});
  MenuEntry testMenu({MenuPage::ledRingTest});
  MenuEntry hiddenMenu({MenuPage::resetConfirm, MenuPage::resetSuccess});

  menuManager = new MenuManager({mainMenu, tempMenu, flowMenu, sensorMenu, functionMenu,
                         relayMenu, testMenu, hiddenMenu});
  if(menuManager == nullptr) {
    Serial.println("MenuManager konnte nicht initialisiert werden");
    error_state = ErrorVars;
//    return;
  }
  // Create Threads
  xTaskCreate(ledTask, "taskOne", 10000, &state->ledState, 1, NULL);
  xTaskCreate(taskScheduleManager, "taskScheduleManager", 10000, NULL, 1, NULL);
  sprintf(state->shortStatus, "OK");
}

void loop() {
  if (state->okPressed) {
    Serial.println("DEBUG: OK gedrückt!");
  }

  // printMemoryUsage();
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    error_state = ErrorTime;
  //  return;
  }
  state->hourOfDay = timeinfo.tm_hour;

  if (state->okPressed && !menuManager->openState()) {
    menuManager->open();
    state->okPressed = false;
  }

  state->flowLiters = round((state->flowImpulseCount * 0.00052) * 100.0) / 100.0;

  if (!menuManager->openState()) {
    lcd->setCursor(0, 0);
    lcd->printf("Status: %s", state->shortStatus);
    lcd->setCursor(0, 1);
    lcd->printf("Wasser: %.2fL", state->flowLiters);

    if (ARRAY_SIZE(state->shortStatus) < 3) {
      lcd->setCursor(11, 0);
      lcd->print(&timeinfo, "%R");
    }

    lcd->setCursor(15, 0); // rechte obere Ecke

    if (WiFi.status() == WL_CONNECTED) {
      int rssi = WiFi.RSSI();
      uint8_t level;
    
      if (rssi > -60)       level = 3;
      else if (rssi > -70)  level = 2;
      else if (rssi > -80)  level = 1;
      else                  level = 0;
    
      lcd->write(level); // Custom-Char für Signalstärke
    } else {
      lcd->write(4); // Disconnected-Symbol
    }    
  }

  sensors->requestTemperatures();
  state->temp1 = sensors->getTempCByIndex(0);
  state->temp2 = sensors->getTempCByIndex(1);
  delay(500);
}

void taskScheduleManager(void *parameter) {
  unsigned long startMillisFlush;
  unsigned long currentMillisFlush;
  bool startup = true;

  while (true) {

    if (startup) {
      relais[0]->turnOn();  // Frischwasserzulauf auf
      delay(1000);          // 1 Sekunde warten
      relais[2]->turnOn();  // Membran-Bypass auf
      delay(5000);          // 300000 = 5m
      relais[2]->turnOff(); // Membran-Bypass zu
      relais[1]->turnOn();  // Abwasserventil auf
      delay(5000);        
      relais[1]->turnOff(); // Abwasserventil zu
      relais[2]->turnOn();  // Membran-Bypass auf
      delay(5000);
      relais[2]->turnOff(); // Membran-Bypass zu
      startup = false;
      startMillisFlush = millis();
    }

    //Feste Spülzeiten
    if (getLocalTime(&timeinfo)) {
      char timestamp[9]; // HH:MM:SS ist 8 + \0
      strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &timeinfo);
      if (state->flushTime1Hour != 0 || state->flushTime1Minute != 0) {
        if (timeinfo.tm_hour == state->flushTime1Hour &&
            timeinfo.tm_min == state->flushTime1Minute &&
            !flushTodayDone[0]) {
    
              Serial.printf("[%s] [INFO] Feste Spülzeit 1 erreicht (eingestellt: %02d:%02d)\n",
              timestamp,
              state->flushTime1Hour,
              state->flushTime1Minute);
              
              //Spülvorgang starten
              state->flushSystem = true;
              flushTodayDone[0] = true;              
        }
      }
    
      if (state->flushTime2Hour != 0 || state->flushTime2Minute != 0) {
        if (timeinfo.tm_hour == state->flushTime2Hour &&
            timeinfo.tm_min == state->flushTime2Minute &&
            !flushTodayDone[1]) {
    
              Serial.printf("[%s] [INFO] Feste Spülzeit 2 erreicht (eingestellt: %02d:%02d)\n",
                timestamp,
                state->flushTime2Hour,
                state->flushTime2Minute);
                
                //Spülvorgang starten
                state->flushSystem = true;
                flushTodayDone[1] = true;                             
        }
      }
    
      // Reset der Flags um Mitternacht
      if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
        flushTodayDone[0] = false;
        flushTodayDone[1] = false;
      }
    }
     
    
    // Alle x Stunden das System spülen
    currentMillisFlush = millis();
    if (state->intervallFlushSystem > 0 &&
      currentMillisFlush - startMillisFlush >= (state->intervallFlushSystem * 3600000)){
          Serial.println("[Info] Flush by Time Delay");

          //Spülvorhang starten
          state->flushSystem = true;
    }

    if (state->fillContainer) {
      Serial.println("[Info] Fill Container");
      relais[3]->turnOn();
      while (true) {
        delay(500);
        if (digitalRead(FLOAT_SENSOR)) {
          relais[3]->turnOff();
          state->fillContainer = false;
          break;
        }
      }
    }

    if (state->flushMembrane) {
      Serial.println("[Info] Flush Membrane");
      sprintf(state->shortStatus, "FLUSH M");
      lcd->clear();
      menuManager->close();
      relais[1]->turnOn();  // Abwasserventil auf
      relais[2]->turnOn();  // Membran-Bypass auf
      delay(5000);        // 300000 = 5m
      relais[2]->turnOff(); // Membran-Bypass zu
      relais[1]->turnOff(); // Abwasserventil zu
      state->flushMembrane = false;
      startMillisFlush = millis();
      sprintf(state->shortStatus, "OK");
      lcd->clear();
    }

    if (state->flushSystem) {
      Serial.println("[Info] Flush System");
      sprintf(state->shortStatus, "FLUSH S");
      lcd->clear();
      menuManager->close();
      relais[0]->turnOn();  // Frischwasserzulauf auf
      delay(1000);          // 1 Sekunde warten
      relais[2]->turnOn();  // Membran-Bypass auf
      delay(60000);          // 300000 = 5m
      relais[2]->turnOff(); // Membran-Bypass zu
      relais[1]->turnOn();  // Abwasserventil auf
      delay(60000);        
      relais[1]->turnOff(); // Abwasserventil zu
      relais[2]->turnOn();  // Membran-Bypass auf
      delay(60000);
      relais[2]->turnOff(); // Membran-Bypass zu
      state->flushSystem = false;
      startMillisFlush = millis();
      sprintf(state->shortStatus, "OK");
      lcd->clear();
    }

    delay(500);
  }
}