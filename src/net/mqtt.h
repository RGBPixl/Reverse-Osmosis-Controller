#pragma once
// MQTT mit Home-Assistant-Discovery. Läuft im Netzwerk-Task.
//
// - Topics und unique_ids der alten Firmware bleiben, damit Entitäten und Verlauf in HA erhalten bleiben.
// - Befehle aus HA gehen als Request an die Steuerung, Einstellungen über den SettingsHub.
// - Zustände werden nur bei Änderung gesendet (schnell wechselnde Werte gedrosselt) und alle 5 min komplett.

#include <PubSubClient.h>
#include <WiFiClient.h>

#include "../app/shared.h"
#include "../core/status.h"

namespace net {

class Mqtt {
 public:
  static constexpr uint32_t kRetryMinMs = 5000;
  static constexpr uint32_t kRetryMaxMs = 60000;
  static constexpr uint32_t kFullPublishMs = 5UL * 60UL * 1000UL;

  Mqtt(app::RequestQueue& requests, app::SettingsHub& settings);

  // Regelmäßig aufrufen. Der Verbindungsaufbau kann einige Sekunden blockieren.
  void loop(uint32_t nowMs, bool wifiUp, const core::SystemStatus& status);

  bool configured() const { return server_[0] != '\0'; }
  bool connected() { return client_.connected(); }

 private:
  enum Slot : uint8_t {
    kState,
    kProblem,
    kProblemText,
    kTemp1,
    kTemp2,
    kLiters,
    kFlowRate,
    kPressure,
    kService,
    kRelay0,
    kRelay1,
    kRelay2,
    kFlushInterval,
    kMembraneInterval,
    kFlushTime1,
    kFlushTime2,
    kRssi,
    kSlotCount
  };

  struct Published {
    bool valid = false;
    uint32_t atMs = 0;
    char value[64] = "";
  };

  bool takeConfig(const config::Settings& s);
  bool connect(uint32_t nowMs);
  void onMessage(char* topic, uint8_t* payload, unsigned int length);
  void publishDiscovery();
  void publishStates(const core::SystemStatus& status, const config::Settings& settings, uint32_t nowMs);
  void publishSlot(Slot slot, const char* topic, const char* value, uint32_t nowMs, uint32_t minIntervalMs = 0);
  void invalidateAll();
  void sendCommand(const core::Command& command);

  app::RequestQueue& requests_;
  app::SettingsHub& settings_;
  WiFiClient wifi_;
  PubSubClient client_;

  // PubSubClient merkt sich nur den Zeiger auf den Servernamen, deshalb eine eigene Kopie.
  char server_[64] = "";
  uint16_t port_ = 1883;
  char user_[32] = "";
  char password_[64] = "";
  uint32_t settingsVersion_ = 0xFFFFFFFF;

  bool attempted_ = false;
  bool wasConnected_ = false;
  uint32_t lastAttemptMs_ = 0;
  uint32_t retryMs_ = kRetryMinMs;
  uint32_t lastFullPublishMs_ = 0;
  Published published_[kSlotCount];
};

}  // namespace net
