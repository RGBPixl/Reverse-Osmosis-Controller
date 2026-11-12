#pragma once
// Netzwerk-Task auf Core 0: WLAN, Einrichtungsportal, NTP, MQTT und Webpanel.
// Darf beim Verbindungsaufbau blockieren, die Steuerung auf Core 1 läuft davon unberührt weiter.
//
// WLAN:
// - Gespeicherte Zugangsdaten: Verbindung im Hintergrund. Bei Abbruch neue Versuche
//   nach 30 s, 60 s, ... bis alle 5 min. Ein Neustart ist nie nötig.
// - Keine Zugangsdaten oder Menüpunkt "WLAN einrichten": offenes WLAN "RO-Setup" mit
//   Einrichtungsseite unter http://192.168.4.1, schließt nach 5 min von selbst.

#include <atomic>
#include <stdint.h>

#include "../app/shared.h"
#include "../core/status.h"
#include "mqtt.h"
#include "web.h"

class WiFiManager;

namespace net {

class Network {
 public:
  static constexpr uint32_t kStackBytes = 12288;
  static constexpr uint32_t kLoopMs = 10;
  static constexpr uint32_t kReconnectMinMs = 30000;
  static constexpr uint32_t kReconnectMaxMs = 300000;
  static constexpr uint32_t kInfoMs = 500;
  static constexpr uint32_t kDiagnosticsMs = 10UL * 60UL * 1000UL;

  Network(app::RequestQueue& requests, app::SettingsHub& settings, app::Shared<core::SystemStatus>& status,
          app::Shared<core::NetworkInfo>& info);

  // Startet den Task. Die Shared-Objekte müssen vorher mit begin() angelegt sein.
  void start();

  // Aus jedem Task aufrufbar
  void requestPortal() { portalRequested_ = true; }

 private:
  static void taskEntry(void* self);
  void run();
  void setupWifi();
  void loopWifi(uint32_t nowMs);
  void startPortal();
  void publishInfo(uint32_t nowMs);

  app::Shared<core::SystemStatus>& status_;
  app::Shared<core::NetworkInfo>& info_;
  Mqtt mqtt_;
  WebPanel web_;
  WiFiManager* wifiManager_ = nullptr;  // wird erst im Task angelegt

  std::atomic<bool> portalRequested_{false};
  bool portalActive_ = false;
  bool wifiWasConnected_ = false;
  bool timeLogged_ = false;
  uint32_t lastReconnectMs_ = 0;
  uint32_t reconnectIntervalMs_ = kReconnectMinMs;
  uint32_t lastInfoMs_ = 0;
  uint32_t lastDiagnosticsMs_ = 0;
};

}  // namespace net
