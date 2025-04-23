#include "mqtt_handler.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "state.h"
#include "relais.h"

extern PubSubClient mqttClient;
extern State* state;
extern String mqtt_server;
extern String mqtt_username;
extern String mqtt_password;
extern String mqtt_client_id;
extern bool mqtt_enabled;
extern Relais* relais[6];

#define FLOAT_SENSOR 25

unsigned long lastMqttAttempt = 0;
const unsigned long mqttRetryInterval = 30000;

float lastTemp1 = -999.0;
float lastTemp2 = -999.0;
float lastFlow = -999.0;

bool lastFloatSensor = false;
bool lastRelayState[6] = {false, false, false, false, false, false};
bool blockExternalRelayControl = false;

float lastSavedLiters = 0.0;
unsigned long lastSaveTime = 0;

// MQTT Callback-Funktion
// Diese Funktion wird aufgerufen, wenn eine Nachricht auf einem abonnierten Thema empfangen wird
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();
  msg.toUpperCase();

  Serial.printf("[MQTT] topic = '%s', payload = '%s'\n", topic, msg.c_str());

  Preferences prefs;

  if (String(topic) == "reverse_osmosis/flush_interval/set") {
    int interval = msg.toInt();
    prefs.begin("Config", false);
    prefs.putInt("iFS", interval);
    prefs.end();
    state->intervallFlushSystem = interval;

  } else if (String(topic) == "reverse_osmosis/flush_time1/set" || String(topic) == "reverse_osmosis/flush_time2/set") {
    String keyPrefix = String(topic).endsWith("flush_time1/set") ? "fT1" : "fT2";
    int hour = -1, minute = -1;
    if (msg.length() == 5 && msg.charAt(2) == ':') {
      hour = msg.substring(0, 2).toInt();
      minute = msg.substring(3, 5).toInt();
    }

    String topicOut = "reverse_osmosis/flush_" + String((keyPrefix == "fT1") ? "time1" : "time2") + "/state";

    if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) {
      prefs.begin("Config", false);
      prefs.putInt((keyPrefix + "H").c_str(), hour);
      prefs.putInt((keyPrefix + "M").c_str(), minute);
      prefs.end();

      if (keyPrefix == "fT1") {
        state->flushTime1Hour = hour;
        state->flushTime1Minute = minute;
      } else if (keyPrefix == "fT2") {
        state->flushTime2Hour = hour;
        state->flushTime2Minute = minute;
      }

      mqttClient.publish(topicOut.c_str(), msg.c_str(), true);
    } else {
      mqttClient.publish(topicOut.c_str(), "00:00", true);
    }

  } else if (String(topic) == "reverse_osmosis/reset_flow/set") {
    if (msg == "PRESS" || msg == "1") {
      state->flowLiters = 0.0f;
      lastSavedLiters = 0.0f;

      prefs.begin("Runtime", false);
      prefs.putFloat("flowLiters", 0.0f);
      prefs.end();

      mqttClient.publish("reverse_osmosis/flowLiters/state", "0.00", true);
      Serial.println("[MQTT] flowLiters wurde auf 0 zurückgesetzt");
    }
  }

  // 🔌 Relais-Steuerung via MQTT
  for (int i = 0; i < 6; i++) {
    String expectedTopic = "reverse_osmosis/relay_" + String(i) + "/set";
    if (String(topic) == expectedTopic) {
      Serial.printf("[MQTT] Steuerung erkannt: Relais %d <- %s\n", i, msg.c_str());

      if (blockExternalRelayControl) {
        Serial.printf("[Info] Relaissteuerung blockiert – Relais %d wird ignoriert\n", i);
        break;
      }

      bool current = relais[i]->isOn();

      if (msg == "ON" && !current) {
        relais[i]->turnOn();
      } else if (msg == "OFF" && current) {
        relais[i]->turnOff();
      }

      mqttClient.publish(("reverse_osmosis/relay_" + String(i) + "/state").c_str(),
                         relais[i]->isOn() ? "ON" : "OFF", true);
      break;
    }
  }
}
 
  
// Funktion zum Veröffentlichen des aktuellen Zustands der Sensoren
// Diese Funktion wird regelmäßig aufgerufen, um die aktuellen Werte zu veröffentlichen
void publishLiveSensorState() {
    bool updated = false;
  
    // Temperatur IN
    if (state->temp1 > -126 && state->temp1 < 85 && abs(state->temp1) > 0.01 && abs(state->temp1 - lastTemp1) >= 0.1) {
      mqttClient.publish("reverse_osmosis/temp1/state", String(state->temp1, 2).c_str(), true);
      lastTemp1 = state->temp1;
      updated = true;
    }
  
    // Temperatur OUT
    if (state->temp2 > -126 && state->temp2 < 85 && abs(state->temp2) > 0.01 && abs(state->temp2 - lastTemp2) >= 0.1) {
      mqttClient.publish("reverse_osmosis/temp2/state", String(state->temp2, 2).c_str(), true);
      lastTemp2 = state->temp2;
      updated = true;
    }
  
    // Flow
    if (state->flowLiters >= 0.0 && state->flowLiters < 10000 && abs(state->flowLiters - lastFlow) >= 0.01) {
      mqttClient.publish("reverse_osmosis/flowLiters/state", String(state->flowLiters, 2).c_str(), true);
      lastFlow = state->flowLiters;
      updated = true;
    }
  
    // Schwimmer
    bool currentFloatSensor = digitalRead(FLOAT_SENSOR);
    if (currentFloatSensor != lastFloatSensor) {
      mqttClient.publish("reverse_osmosis/float_sensor/state", currentFloatSensor ? "ON" : "OFF", true);
      lastFloatSensor = currentFloatSensor;
      updated = true;
    }
  
    if (updated) {
      Serial.println("[MQTT] Live-Daten aktualisiert");
    }
}
  
  

void connectToMqtt() {
    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 10000;
  
    while (!mqttClient.connected() && millis() - startAttemptTime < timeout) {
      Serial.print("Connecting to MQTT... ");
  
      if (mqttClient.connect(
            mqtt_client_id.c_str(),
            mqtt_username.c_str(),
            mqtt_password.c_str(),
            "reverse_osmosis/status",  // Will/Birth Topic
            0,                         // QoS
            true,                      // retained
            "offline"))               // Will Payload (offline)
      {
        // Send "online" immediately after successful connection
        mqttClient.publish("reverse_osmosis/status", "online", true);
  
        Serial.println("connected!");
        mqttClient.subscribe("reverse_osmosis/flush_interval/set");
        mqttClient.subscribe("reverse_osmosis/flush_time1/set");
        mqttClient.subscribe("reverse_osmosis/flush_time2/set");
        mqttClient.subscribe("reverse_osmosis/reset_flow/set");

        for (int i = 0; i < 6; i++) {
            mqttClient.subscribe(("reverse_osmosis/relay_" + String(i) + "/set").c_str());
          }
          
        return;
      } else {
        Serial.print("failed (rc=");
        Serial.print(mqttClient.state());
        Serial.println("), retrying...");
        delay(1000);
      }
    }
  
    Serial.println("MQTT: Verbindung fehlgeschlagen. Weiter im Setup.");
  }

void setupMqtt() {
    if (mqtt_server.isEmpty()) {
      Serial.println("MQTT-Server nicht gesetzt – MQTT wird übersprungen.");
      return;
    }
  
    mqttClient.setServer(mqtt_server.c_str(), 1883);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024);
    connectToMqtt();
  
    if (mqttClient.connected()) {
      publishMqttDiscovery();
      publishMqttState();
  
      // Sende initialen FLOAT_SENSOR-Zustand
      bool initFloat = digitalRead(FLOAT_SENSOR);
      mqttClient.publish("reverse_osmosis/float_sensor/state", initFloat ? "ON" : "OFF", true);
      lastFloatSensor = initFloat;
  
      publishLiveSensorState();
    } else {
      Serial.println("MQTT-Verbindung nicht hergestellt – Discovery wird nicht gesendet.");
    }
  }

  void handleMqttLoop() {
    if (!mqtt_enabled) return;
  
    if (!mqttClient.connected()) {
      unsigned long now = millis();
      if (now - lastMqttAttempt > mqttRetryInterval) {
        lastMqttAttempt = now;
        connectToMqtt();
      }
      return;
    }
  
    mqttClient.loop();
    publishLiveSensorState();
    checkAndPublishRelayStates();
  
    // Speicherung von flowLiters nur bei relevanter Änderung und Intervall
    const unsigned long saveInterval = 30 * 60 * 1000; // 30 Minuten
  
    if (millis() - lastSaveTime >= saveInterval && abs(state->flowLiters - lastSavedLiters) >= 0.5) {
      Preferences prefs;
      prefs.begin("Runtime", false);
      prefs.putFloat("flowLiters", state->flowLiters);
      prefs.end();
  
      lastSavedLiters = state->flowLiters;
      lastSaveTime = millis();
  
      Serial.printf("[MQTT] flowLiters gespeichert: %.2f L\n", state->flowLiters);
    }
  }
  

  void publishMqttDiscovery() {
    String baseDevice = "\"device\":{\"identifiers\":[\"reverse_osmosis_controller\"],\"name\":\"Umkehrosmoseanlage\",\"manufacturer\":\"RGB Pixl\",\"model\":\"ESP32 MQTT Controller\",\"sw_version\":\"1.0.0\"}";
  
    mqttClient.publish("homeassistant/number/ro_flush_interval/config",
      ("{\"name\":\"System Spülintervall\",\"uniq_id\":\"ro_flush_interval\",\"stat_t\":\"reverse_osmosis/flush_interval/state\",\"cmd_t\":\"reverse_osmosis/flush_interval/set\",\"min\":0,\"max\":24,\"step\":1,\"unit_of_meas\":\"h\",\"dev_cla\":\"duration\",\"mode\":\"slider\",\"qos\":0," + baseDevice + "}").c_str(), true);
  
    mqttClient.publish("homeassistant/text/ro_flush_time1/config",
      ("{\"name\":\"Spülzeit 1\",\"uniq_id\":\"ro_flush_time1\",\"stat_t\":\"reverse_osmosis/flush_time1/state\",\"cmd_t\":\"reverse_osmosis/flush_time1/set\",\"mode\":\"text\",\"qos\":0," + baseDevice + "}").c_str(), true);
  
    mqttClient.publish("homeassistant/text/ro_flush_time2/config",
      ("{\"name\":\"Spülzeit 2\",\"uniq_id\":\"ro_flush_time2\",\"stat_t\":\"reverse_osmosis/flush_time2/state\",\"cmd_t\":\"reverse_osmosis/flush_time2/set\",\"mode\":\"text\",\"qos\":0," + baseDevice + "}").c_str(), true);
  
    mqttClient.publish("homeassistant/sensor/ro_temp1/config",
      ("{\"name\":\"Temperatur IN\",\"uniq_id\":\"ro_temp1\",\"stat_t\":\"reverse_osmosis/temp1/state\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"qos\":0," + baseDevice + "}").c_str(), true);
  
    mqttClient.publish("homeassistant/sensor/ro_temp2/config",
      ("{\"name\":\"Temperatur OUT\",\"uniq_id\":\"ro_temp2\",\"stat_t\":\"reverse_osmosis/temp2/state\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"qos\":0," + baseDevice + "}").c_str(), true);
  
    mqttClient.publish("homeassistant/sensor/ro_flowLiters/config",
      ("{\"name\":\"Wassermenge\",\"uniq_id\":\"ro_flowLiters\",\"stat_t\":\"reverse_osmosis/flowLiters/state\",\"unit_of_meas\":\"L\",\"dev_cla\":\"water\",\"qos\":0," + baseDevice + "}").c_str(), true);
  
    mqttClient.publish("homeassistant/binary_sensor/ro_float/config",
      ("{\"name\":\"Schwimmer\",\"uniq_id\":\"ro_float\",\"stat_t\":\"reverse_osmosis/float_sensor/state\",\"dev_cla\":\"moisture\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"qos\":0," + baseDevice + "}").c_str(), true);
  
    mqttClient.publish("homeassistant/button/reset_flow/config",
      ("{\"name\":\"Zähler zurücksetzen\",\"uniq_id\":\"ro_reset_flow\",\"cmd_t\":\"reverse_osmosis/reset_flow/set\",\"payload_press\":\"PRESS\",\"qos\":0," + baseDevice + "}").c_str(), true);
  
    // 🔌 Relais Discovery (0–5)
    for (int i = 0; i < 6; i++) {
      String relayName = "Relais " + String(i + 1); // Fallback
      switch (i) {
        case 0: relayName = "Frischwasser"; break;
        case 1: relayName = "Abwasser"; break;
        case 2: relayName = "Membran-Bypass"; break;
        case 3: relayName = "Container"; break;
        case 4: relayName = "Reserve 1"; break;
        case 5: relayName = "Reserve 2"; break;
      }
  
      String configTopic = "homeassistant/switch/relay_" + String(i) + "/config";
      String payload = "{";
      payload += "\"name\":\"" + relayName + "\",";
      payload += "\"uniq_id\":\"ro_relay_" + String(i) + "\",";
      payload += "\"cmd_t\":\"reverse_osmosis/relay_" + String(i) + "/set\",";
      payload += "\"stat_t\":\"reverse_osmosis/relay_" + String(i) + "/state\",";
      payload += "\"payload_on\":\"ON\",";
      payload += "\"payload_off\":\"OFF\",";
      payload += "\"retain\":true,";
      payload += "\"qos\":0,";
      payload += baseDevice;
      payload += "}";
  
      mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    }
  }
 

void publishMqttState() {
  Preferences prefs;
  prefs.begin("Config", true);
  int interval = prefs.getInt("iFS", 0);
  int h1 = prefs.getInt("fT1H", 0);
  int m1 = prefs.getInt("fT1M", 0);
  int h2 = prefs.getInt("fT2H", 0);
  int m2 = prefs.getInt("fT2M", 0);
  prefs.end();

  mqttClient.publish("reverse_osmosis/flush_interval/state", String(interval).c_str(), true);

  char t1[6], t2[6];
  sprintf(t1, "%02d:%02d", h1, m1);
  sprintf(t2, "%02d:%02d", h2, m2);

  mqttClient.publish("reverse_osmosis/flush_time1/state", t1, true);
  mqttClient.publish("reverse_osmosis/flush_time2/state", t2, true);
}

void checkAndPublishRelayStates() {
    for (int i = 0; i < 6; i++) {
      bool current = relais[i]->isOn();
      if (current != lastRelayState[i]) {
        String topic = "reverse_osmosis/relay_" + String(i) + "/state";
        mqttClient.publish(topic.c_str(), current ? "ON" : "OFF", true);
        lastRelayState[i] = current;
      }
    }
  }
  