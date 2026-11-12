#include "mqtt.h"

#include <string.h>

#include "../config/network.h"
#include "../config/version.h"
#include "../util/log.h"
#include "protocol.h"

namespace net {

namespace {

constexpr const char* kPrefix = "reverse_osmosis/";

// Home-Assistant-Entitäten. node und uniqueId der alten Firmware sind unverändert übernommen.
struct Entity {
  const char* component;
  const char* node;
  const char* uniqueId;
  const char* name;
  const char* fields;
};

#define RO_STATE(t) "\"stat_t\":\"reverse_osmosis/" t "/state\""
#define RO_CMD(t) "\"cmd_t\":\"reverse_osmosis/" t "/set\""
#define RO_ON_OFF "\"pl_on\":\"ON\",\"pl_off\":\"OFF\""
#define RO_PRESS "\"pl_prs\":\"PRESS\""

const Entity kEntities[] = {
    // Einstellungen
    {"number", "ro_flush_interval", "ro_flush_interval", "System Spülintervall",
     RO_STATE("flush_interval") "," RO_CMD("flush_interval") ",\"min\":0,\"max\":168,\"step\":1,"
     "\"unit_of_meas\":\"h\",\"mode\":\"box\",\"ic\":\"mdi:timer-sync-outline\""},
    {"number", "ro_membrane_interval", "ro_membrane_interval", "Membran Spülintervall",
     RO_STATE("membrane_interval") "," RO_CMD("membrane_interval") ",\"min\":0,\"max\":168,\"step\":1,"
     "\"unit_of_meas\":\"h\",\"mode\":\"box\",\"ic\":\"mdi:timer-sync-outline\""},
    {"text", "ro_flush_time1", "ro_flush_time1", "Spülzeit 1",
     RO_STATE("flush_time1") "," RO_CMD("flush_time1") ",\"max\":5,\"mode\":\"text\",\"ic\":\"mdi:clock-outline\""},
    {"text", "ro_flush_time2", "ro_flush_time2", "Spülzeit 2",
     RO_STATE("flush_time2") "," RO_CMD("flush_time2") ",\"max\":5,\"mode\":\"text\",\"ic\":\"mdi:clock-outline\""},

    // Messwerte
    {"sensor", "ro_temp1", "ro_temp1", "Temperatur IN",
     RO_STATE("temp1") ",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"stat_cla\":\"measurement\""},
    {"sensor", "ro_temp2", "ro_temp2", "Temperatur OUT",
     RO_STATE("temp2") ",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"stat_cla\":\"measurement\""},
    {"sensor", "ro_flowLiters", "ro_flowLiters", "Wassermenge",
     RO_STATE("flowLiters") ",\"unit_of_meas\":\"L\",\"dev_cla\":\"water\",\"stat_cla\":\"total_increasing\""},
    {"sensor", "ro_flow_rate", "ro_flow_rate", "Durchfluss",
     RO_STATE("flow_rate") ",\"unit_of_meas\":\"L/min\",\"dev_cla\":\"volume_flow_rate\",\"stat_cla\":\"measurement\""},
    {"binary_sensor", "ro_pressure", "ro_pressure", "Druck", RO_STATE("pressure") "," RO_ON_OFF ",\"ic\":\"mdi:gauge\""},

    // Zustand und Meldungen
    {"sensor", "ro_state", "ro_state", "Zustand", RO_STATE("state") ",\"ic\":\"mdi:water-pump\""},
    {"binary_sensor", "ro_problem", "ro_problem", "Problem", RO_STATE("problem") "," RO_ON_OFF ",\"dev_cla\":\"problem\""},
    {"sensor", "ro_problem_text", "ro_problem_text", "Meldung",
     RO_STATE("problem_text") ",\"ic\":\"mdi:message-alert-outline\""},

    // Bedienung
    {"switch", "ro_service", "ro_service", "Service-Modus",
     RO_STATE("service") "," RO_CMD("service") "," RO_ON_OFF ",\"ic\":\"mdi:wrench\""},
    {"switch", "relay_0", "ro_relay_0", "Frischwasser",
     RO_STATE("relay_0") "," RO_CMD("relay_0") "," RO_ON_OFF ",\"ic\":\"mdi:valve\""},
    {"switch", "relay_1", "ro_relay_1", "Abwasser",
     RO_STATE("relay_1") "," RO_CMD("relay_1") "," RO_ON_OFF ",\"ic\":\"mdi:valve\""},
    {"switch", "relay_2", "ro_relay_2", "Membran-Bypass",
     RO_STATE("relay_2") "," RO_CMD("relay_2") "," RO_ON_OFF ",\"ic\":\"mdi:valve\""},
    {"button", "reset_flow", "ro_reset_flow", "Zähler zurücksetzen", RO_CMD("reset_flow") "," RO_PRESS},
    {"button", "ro_flush_system", "ro_flush_system", "Systemspülung starten",
     RO_CMD("flush_system") "," RO_PRESS ",\"ic\":\"mdi:water-sync\""},
    {"button", "ro_flush_membrane", "ro_flush_membrane", "Membranspülung starten",
     RO_CMD("flush_membrane") "," RO_PRESS ",\"ic\":\"mdi:water-sync\""},
    {"button", "ro_abort_flush", "ro_abort_flush", "Spülung abbrechen",
     RO_CMD("abort_flush") "," RO_PRESS ",\"ic\":\"mdi:stop-circle-outline\""},
    {"button", "ro_ack_fault", "ro_ack_fault", "Fehler quittieren",
     RO_CMD("ack_fault") "," RO_PRESS ",\"ic\":\"mdi:alert-remove-outline\""},

    // Diagnose
    {"sensor", "ro_wifi_rssi", "ro_wifi_rssi", "WLAN-Signal",
     RO_STATE("wifi_rssi") ",\"unit_of_meas\":\"dBm\",\"dev_cla\":\"signal_strength\",\"stat_cla\":\"measurement\","
     "\"ent_cat\":\"diagnostic\""},
};

// Entitäten der alten Firmware für nicht angeschlossene Hardware (siehe docs/spaetere-features.md)
const char* const kRemovedEntities[][2] = {
    {"binary_sensor", "ro_float"},
    {"switch", "relay_3"},
    {"switch", "relay_4"},
    {"switch", "relay_5"},
};

#undef RO_STATE
#undef RO_CMD
#undef RO_ON_OFF
#undef RO_PRESS

}  // namespace

Mqtt::Mqtt(app::RequestQueue& requests, app::SettingsHub& settings)
    : requests_(requests), settings_(settings), client_(wifi_) {}

bool Mqtt::takeConfig(const config::Settings& s) {
  const bool changed = strcmp(server_, s.mqttServer) != 0 || port_ != s.mqttPort || strcmp(user_, s.mqttUser) != 0 ||
                       strcmp(password_, s.mqttPassword) != 0;
  if (changed) {
    snprintf(server_, sizeof(server_), "%s", s.mqttServer);
    port_ = s.mqttPort;
    snprintf(user_, sizeof(user_), "%s", s.mqttUser);
    snprintf(password_, sizeof(password_), "%s", s.mqttPassword);
  }
  return changed;
}

void Mqtt::loop(uint32_t nowMs, bool wifiUp, const core::SystemStatus& status) {
  uint32_t version = 0;
  const config::Settings settings = settings_.get(&version);
  if (version != settingsVersion_) {
    settingsVersion_ = version;
    if (takeConfig(settings)) {
      if (client_.connected()) {
        util::logf("MQTT", "Zugangsdaten geaendert, verbinde neu");
        client_.disconnect();
      }
      attempted_ = false;
      retryMs_ = kRetryMinMs;
    }
  }

  if (!configured() || !wifiUp) {
    if (client_.connected()) client_.disconnect();
    if (wasConnected_) {
      util::logf("MQTT", "Getrennt (%s)", configured() ? "kein WLAN" : "kein Server eingestellt");
      wasConnected_ = false;
    }
    return;
  }

  if (!client_.connected()) {
    if (wasConnected_) {
      util::logf("MQTT", "Verbindung verloren");
      wasConnected_ = false;
    }
    if (!attempted_ || nowMs - lastAttemptMs_ >= retryMs_) {
      attempted_ = true;
      lastAttemptMs_ = nowMs;
      if (connect(nowMs)) {
        retryMs_ = kRetryMinMs;
      } else {
        retryMs_ *= 2;
        if (retryMs_ > kRetryMaxMs) retryMs_ = kRetryMaxMs;
        util::logf("MQTT", "Neuer Versuch in %lu s", static_cast<unsigned long>(retryMs_ / 1000));
      }
    }
    return;
  }

  client_.loop();
  publishStates(status, settings, nowMs);
}

bool Mqtt::connect(uint32_t nowMs) {
  util::logf("MQTT", "Verbinde mit %s:%u", server_, static_cast<unsigned>(port_));
  client_.setServer(server_, port_);
  client_.setBufferSize(1024);
  client_.setKeepAlive(30);
  client_.setSocketTimeout(5);
  client_.setCallback([this](char* topic, uint8_t* payload, unsigned int length) { onMessage(topic, payload, length); });

  const bool withUser = user_[0] != '\0';
  const bool ok = client_.connect(config::kMqttClientId, withUser ? user_ : nullptr, withUser ? password_ : nullptr,
                                  config::kMqttAvailabilityTopic, 0, true, "offline");
  if (!ok) {
    util::logf("MQTT", "Verbindung fehlgeschlagen (rc=%d)", client_.state());
    return false;
  }

  client_.publish(config::kMqttAvailabilityTopic, "online", true);

  // Die alte Firmware hat Relais-Befehle mit retain senden lassen. Solche Reste auf dem Broker löschen,
  // damit sie nicht bei jeder Verbindung erneut ankommen.
  for (int i = 0; i < 6; ++i) {
    char topic[40];
    snprintf(topic, sizeof(topic), "%srelay_%d/set", kPrefix, i);
    client_.publish(topic, "", true);
  }

  client_.subscribe("reverse_osmosis/+/set");
  publishDiscovery();
  invalidateAll();
  lastFullPublishMs_ = nowMs;
  wasConnected_ = true;
  util::logf("MQTT", "Verbunden");
  return true;
}

void Mqtt::publishDiscovery() {
  char topic[96];
  char payload[768];
  for (const Entity& e : kEntities) {
    snprintf(topic, sizeof(topic), "homeassistant/%s/%s/config", e.component, e.node);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"%s\",\"uniq_id\":\"%s\",%s,\"avty_t\":\"%s\","
             "\"dev\":{\"ids\":[\"reverse_osmosis_controller\"],\"name\":\"Umkehrosmoseanlage\","
             "\"mf\":\"RGB Pixl\",\"mdl\":\"ESP32 Reverse-Osmosis-Controller\",\"sw\":\"%s\"}}",
             e.name, e.uniqueId, e.fields, config::kMqttAvailabilityTopic, config::kFirmwareVersion);
    if (!client_.publish(topic, payload, true)) {
      util::logf("MQTT", "Discovery fuer %s fehlgeschlagen", e.uniqueId);
    }
  }
  // Leere, gespeicherte Nachricht entfernt die Entität in HA.
  for (const auto& removed : kRemovedEntities) {
    snprintf(topic, sizeof(topic), "homeassistant/%s/%s/config", removed[0], removed[1]);
    client_.publish(topic, "", true);
  }
}

void Mqtt::invalidateAll() {
  for (Published& p : published_) {
    p.valid = false;
  }
}

void Mqtt::publishSlot(Slot slot, const char* topic, const char* value, uint32_t nowMs, uint32_t minIntervalMs) {
  Published& p = published_[slot];
  if (p.valid && strcmp(p.value, value) == 0) {
    return;
  }
  if (p.valid && nowMs - p.atMs < minIntervalMs) {
    return;  // gedrosselt, wird später gesendet
  }
  if (client_.publish(topic, value, true)) {
    p.valid = true;
    p.atMs = nowMs;
    snprintf(p.value, sizeof(p.value), "%s", value);
  }
}

void Mqtt::publishStates(const core::SystemStatus& status, const config::Settings& settings, uint32_t nowMs) {
  if (nowMs - lastFullPublishMs_ >= kFullPublishMs) {
    lastFullPublishMs_ = nowMs;
    invalidateAll();
  }

  const core::ControllerStatus& c = status.controller;
  const core::Measurements& m = status.measurements;
  char value[64];

  stateText(c, value, sizeof(value));
  publishSlot(kState, "reverse_osmosis/state/state", value, nowMs);
  publishSlot(kProblem, "reverse_osmosis/problem/state", hasProblem(c) ? "ON" : "OFF", nowMs);
  problemText(c, value, sizeof(value));
  publishSlot(kProblemText, "reverse_osmosis/problem_text/state", value, nowMs);

  if (m.tempValid[0]) {
    snprintf(value, sizeof(value), "%.1f", static_cast<double>(m.tempC[0]));
    publishSlot(kTemp1, "reverse_osmosis/temp1/state", value, nowMs, 10000);
  }
  if (m.tempValid[1]) {
    snprintf(value, sizeof(value), "%.1f", static_cast<double>(m.tempC[1]));
    publishSlot(kTemp2, "reverse_osmosis/temp2/state", value, nowMs, 10000);
  }
  snprintf(value, sizeof(value), "%.2f", m.totalLiters);
  publishSlot(kLiters, "reverse_osmosis/flowLiters/state", value, nowMs, 10000);
  snprintf(value, sizeof(value), "%.2f", static_cast<double>(m.flowLpm));
  publishSlot(kFlowRate, "reverse_osmosis/flow_rate/state", value, nowMs, 5000);
  publishSlot(kPressure, "reverse_osmosis/pressure/state", m.pressurized ? "ON" : "OFF", nowMs);

  publishSlot(kService, "reverse_osmosis/service/state", c.mode == core::Mode::Service ? "ON" : "OFF", nowMs);
  publishSlot(kRelay0, "reverse_osmosis/relay_0/state", core::isOpen(c.valves, core::Valve::FreshWater) ? "ON" : "OFF",
              nowMs);
  publishSlot(kRelay1, "reverse_osmosis/relay_1/state", core::isOpen(c.valves, core::Valve::WasteWater) ? "ON" : "OFF",
              nowMs);
  publishSlot(kRelay2, "reverse_osmosis/relay_2/state",
              core::isOpen(c.valves, core::Valve::MembraneBypass) ? "ON" : "OFF", nowMs);

  snprintf(value, sizeof(value), "%u", static_cast<unsigned>(settings.systemFlushIntervalH));
  publishSlot(kFlushInterval, "reverse_osmosis/flush_interval/state", value, nowMs);
  snprintf(value, sizeof(value), "%u", static_cast<unsigned>(settings.membraneFlushIntervalH));
  publishSlot(kMembraneInterval, "reverse_osmosis/membrane_interval/state", value, nowMs);
  formatFlushTime(settings.flushTimes[0], value, sizeof(value));
  publishSlot(kFlushTime1, "reverse_osmosis/flush_time1/state", value, nowMs);
  formatFlushTime(settings.flushTimes[1], value, sizeof(value));
  publishSlot(kFlushTime2, "reverse_osmosis/flush_time2/state", value, nowMs);

  if (status.network.wifiConnected) {
    snprintf(value, sizeof(value), "%d", static_cast<int>(status.network.rssi));
    publishSlot(kRssi, "reverse_osmosis/wifi_rssi/state", value, nowMs, 60000);
  }
}

void Mqtt::sendCommand(const core::Command& command) {
  if (!requests_.push(app::Request::controller(command))) {
    util::logf("MQTT", "Befehl verworfen: Warteschlange voll");
  }
}

void Mqtt::onMessage(char* topic, uint8_t* payload, unsigned int length) {
  const size_t prefixLength = strlen(kPrefix);
  if (strncmp(topic, kPrefix, prefixLength) != 0) {
    return;
  }
  const char* name = topic + prefixLength;

  char text[64];
  copyPayload(payload, length, text, sizeof(text));
  if (text[0] == '\0' && strncmp(name, "flush_time", 10) != 0) {
    return;  // leere Nachricht, z. B. vom Aufräumen gespeicherter Befehle
  }
  util::logf("MQTT", "Empfangen: %s = '%s'", name, text);

  bool on = false;
  uint8_t hours = 0;

  if (strcmp(name, "flush_interval/set") == 0) {
    if (parseHours(text, config::kMaxFlushIntervalH, hours)) {
      settings_.modify([hours](config::Settings& s) { s.systemFlushIntervalH = hours; });
    }
    published_[kFlushInterval].valid = false;
  } else if (strcmp(name, "membrane_interval/set") == 0) {
    if (parseHours(text, config::kMaxFlushIntervalH, hours)) {
      settings_.modify([hours](config::Settings& s) { s.membraneFlushIntervalH = hours; });
    }
    published_[kMembraneInterval].valid = false;
  } else if (strcmp(name, "flush_time1/set") == 0 || strcmp(name, "flush_time2/set") == 0) {
    const size_t index = name[10] == '1' ? 0 : 1;
    config::FlushTime time = settings_.get().flushTimes[index];
    if (parseFlushTime(text, time)) {
      settings_.modify([index, time](config::Settings& s) { s.flushTimes[index] = time; });
    } else {
      util::logf("MQTT", "Ungueltige Spuelzeit '%s', erwartet HH:MM oder --:--", text);
    }
    published_[index == 0 ? kFlushTime1 : kFlushTime2].valid = false;
  } else if (strcmp(name, "reset_flow/set") == 0) {
    if (isPress(text) && !requests_.push(app::Request::resetCounter())) {
      util::logf("MQTT", "Befehl verworfen: Warteschlange voll");
    }
    published_[kLiters].valid = false;
  } else if (strcmp(name, "flush_system/set") == 0) {
    if (isPress(text)) sendCommand(core::Command::startFlush(core::Program::System));
  } else if (strcmp(name, "flush_membrane/set") == 0) {
    if (isPress(text)) sendCommand(core::Command::startFlush(core::Program::Membrane));
  } else if (strcmp(name, "abort_flush/set") == 0) {
    if (isPress(text)) sendCommand(core::Command::abortFlush());
  } else if (strcmp(name, "ack_fault/set") == 0) {
    if (isPress(text)) sendCommand(core::Command::acknowledgeFault());
  } else if (strcmp(name, "service/set") == 0) {
    if (parseOnOff(text, on)) sendCommand(core::Command::serviceMode(on));
    published_[kService].valid = false;
  } else if (strncmp(name, "relay_", 6) == 0 && strcmp(name + 7, "/set") == 0 && name[6] >= '0' && name[6] <= '2') {
    const int index = name[6] - '0';
    if (parseOnOff(text, on)) sendCommand(core::Command::setValve(static_cast<core::Valve>(index), on));
    // Den echten Zustand zurückmelden, auch wenn der Befehl außerhalb des Service-Modus abgelehnt wird.
    published_[kRelay0 + index].valid = false;
  }
}

}  // namespace net
