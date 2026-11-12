#include "settings_store.h"

#include <Preferences.h>
#include <math.h>

#include "../util/log.h"

namespace config {

namespace {

// Namespaces und Schlüssel. Maximal 15 Zeichen pro Name.
// "alt" = schon von der Firmware vor dem Refactoring benutzt, Typ darf sich nicht ändern.
constexpr const char* kNsConfig = "Config";
constexpr const char* kKeySystemInterval = "iFS";    // alt, int
constexpr const char* kKeyMembraneInterval = "iFM";  // alt, int
constexpr const char* kKeyFlushHour[kFlushTimeCount] = {"fT1H", "fT2H"};    // alt, int
constexpr const char* kKeyFlushMinute[kFlushTimeCount] = {"fT1M", "fT2M"};  // alt, int
constexpr const char* kKeyFlushEnabled[kFlushTimeCount] = {"fT1On", "fT2On"};
constexpr const char* kKeySystemStep = "sysStepS";
constexpr const char* kKeyMembraneFlush = "memFlushS";
constexpr const char* kKeyPressureBuild = "pBuildMaxS";
constexpr const char* kKeyInletMaxOpen = "inMaxOpenS";
constexpr const char* kKeyDrawThreshold = "drawLpm";
constexpr const char* kKeyLitersPerPulse = "lPerPulse";

constexpr const char* kNsMqtt = "MQTT";
constexpr const char* kKeyMqttServer = "mqtt_server";  // alt, String
constexpr const char* kKeyMqttUser = "mqtt_user";      // alt, String
constexpr const char* kKeyMqttPassword = "mqtt_pass";  // alt, String
constexpr const char* kKeyMqttPort = "mqtt_port";

constexpr const char* kNsRuntime = "Runtime";
constexpr const char* kKeyLegacyLiters = "flowLiters";  // alt, float
constexpr const char* kKeyTotalLiters = "totalLiters";  // double, genauer als float
constexpr const char* kKeyLastBoot = "lastBoot";

uint8_t toU8(int32_t value) { return value < 0 ? 0 : (value > 255 ? 255 : static_cast<uint8_t>(value)); }
uint16_t toU16(int32_t value) { return value < 0 ? 0 : (value > 65535 ? 65535 : static_cast<uint16_t>(value)); }

}  // namespace

Settings loadSettings() {
  Settings s;
  Preferences prefs;

  // Ein noch nicht vorhandener Namespace lässt sich read-only nicht öffnen. Dann gelten die Standards.
  if (prefs.begin(kNsConfig, true)) {
    s.systemFlushIntervalH = toU8(prefs.getInt(kKeySystemInterval, s.systemFlushIntervalH));
    s.membraneFlushIntervalH = toU8(prefs.getInt(kKeyMembraneInterval, s.membraneFlushIntervalH));

    for (size_t i = 0; i < kFlushTimeCount; ++i) {
      FlushTime& t = s.flushTimes[i];
      t.hour = toU8(prefs.getInt(kKeyFlushHour[i], 0));
      t.minute = toU8(prefs.getInt(kKeyFlushMinute[i], 0));
      // Die alte Firmware kannte kein "aus": 00:00 bedeutete deaktiviert.
      t.enabled = prefs.isKey(kKeyFlushEnabled[i]) ? prefs.getBool(kKeyFlushEnabled[i], false)
                                                   : (t.hour != 0 || t.minute != 0);
    }

    s.systemFlushStepS = toU16(prefs.getInt(kKeySystemStep, s.systemFlushStepS));
    s.membraneFlushS = toU16(prefs.getInt(kKeyMembraneFlush, s.membraneFlushS));
    s.pressureBuildMaxS = toU16(prefs.getInt(kKeyPressureBuild, s.pressureBuildMaxS));
    s.inletMaxOpenS = toU16(prefs.getInt(kKeyInletMaxOpen, s.inletMaxOpenS));
    s.drawThresholdLpm = prefs.getFloat(kKeyDrawThreshold, s.drawThresholdLpm);
    s.litersPerPulse = prefs.getFloat(kKeyLitersPerPulse, s.litersPerPulse);
    prefs.end();
  }

  if (prefs.begin(kNsMqtt, true)) {
    prefs.getString(kKeyMqttServer, s.mqttServer, sizeof(s.mqttServer));
    prefs.getString(kKeyMqttUser, s.mqttUser, sizeof(s.mqttUser));
    prefs.getString(kKeyMqttPassword, s.mqttPassword, sizeof(s.mqttPassword));
    s.mqttPort = toU16(prefs.getInt(kKeyMqttPort, s.mqttPort));
    prefs.end();
  }

  if (sanitize(s)) {
    util::logf("Config", "Ungueltige Werte im Speicher wurden korrigiert");
  }
  return s;
}

bool saveSettings(const Settings& input) {
  Settings s = input;
  sanitize(s);
  Preferences prefs;

  if (!prefs.begin(kNsConfig, false)) {
    util::logf("Config", "NVS-Namespace %s nicht beschreibbar", kNsConfig);
    return false;
  }
  prefs.putInt(kKeySystemInterval, s.systemFlushIntervalH);
  prefs.putInt(kKeyMembraneInterval, s.membraneFlushIntervalH);
  for (size_t i = 0; i < kFlushTimeCount; ++i) {
    prefs.putInt(kKeyFlushHour[i], s.flushTimes[i].hour);
    prefs.putInt(kKeyFlushMinute[i], s.flushTimes[i].minute);
    prefs.putBool(kKeyFlushEnabled[i], s.flushTimes[i].enabled);
  }
  prefs.putInt(kKeySystemStep, s.systemFlushStepS);
  prefs.putInt(kKeyMembraneFlush, s.membraneFlushS);
  prefs.putInt(kKeyPressureBuild, s.pressureBuildMaxS);
  prefs.putInt(kKeyInletMaxOpen, s.inletMaxOpenS);
  prefs.putFloat(kKeyDrawThreshold, s.drawThresholdLpm);
  prefs.putFloat(kKeyLitersPerPulse, s.litersPerPulse);
  prefs.end();

  if (!prefs.begin(kNsMqtt, false)) {
    util::logf("Config", "NVS-Namespace %s nicht beschreibbar", kNsMqtt);
    return false;
  }
  prefs.putString(kKeyMqttServer, s.mqttServer);
  prefs.putString(kKeyMqttUser, s.mqttUser);
  prefs.putString(kKeyMqttPassword, s.mqttPassword);
  prefs.putInt(kKeyMqttPort, s.mqttPort);
  prefs.end();

  util::logf("Config", "Einstellungen gespeichert");
  return true;
}

void eraseSettings() {
  Preferences prefs;
  if (prefs.begin(kNsConfig, false)) {
    prefs.clear();
    prefs.end();
  }
  if (prefs.begin(kNsMqtt, false)) {
    prefs.clear();
    prefs.end();
  }
  util::logf("Config", "Werksreset: Einstellungen geloescht");
}

double loadTotalLiters() {
  Preferences prefs;
  double liters = 0.0;
  if (prefs.begin(kNsRuntime, true)) {
    liters = prefs.isKey(kKeyTotalLiters) ? prefs.getDouble(kKeyTotalLiters, 0.0)
                                          : prefs.getFloat(kKeyLegacyLiters, 0.0f);
    prefs.end();
  }
  if (!isfinite(liters) || liters < 0.0) {
    liters = 0.0;
  }
  return liters;
}

bool saveTotalLiters(double liters) {
  Preferences prefs;
  if (!prefs.begin(kNsRuntime, false)) {
    return false;
  }
  prefs.putDouble(kKeyTotalLiters, liters);
  // Alten Schlüssel mitschreiben, damit die Firmware von master nach einem Zurückflashen den Stand kennt.
  prefs.putFloat(kKeyLegacyLiters, static_cast<float>(liters));
  prefs.end();
  return true;
}

uint32_t loadLastBootEpoch() {
  Preferences prefs;
  uint32_t epoch = 0;
  if (prefs.begin(kNsRuntime, true)) {
    epoch = prefs.getULong(kKeyLastBoot, 0);
    prefs.end();
  }
  return epoch;
}

bool saveLastBootEpoch(uint32_t epoch) {
  Preferences prefs;
  if (!prefs.begin(kNsRuntime, false)) {
    return false;
  }
  prefs.putULong(kKeyLastBoot, epoch);
  prefs.end();
  return true;
}

}  // namespace config
