#include "network.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include "../config/network.h"
#include "../util/clock.h"
#include "../util/log.h"

namespace net {

Network::Network(app::RequestQueue& requests, app::SettingsHub& settings, app::Shared<core::SystemStatus>& status,
                 app::Shared<core::NetworkInfo>& info)
    : status_(status), info_(info), mqtt_(requests, settings), web_(requests, settings, status) {}

void Network::start() {
  // Core 0, auf dem auch der WLAN-Stack läuft. Die Steuerung (Arduino-loop) läuft auf Core 1.
  xTaskCreatePinnedToCore(taskEntry, "network", kStackBytes, this, 1, nullptr, 0);
}

void Network::taskEntry(void* self) { static_cast<Network*>(self)->run(); }

void Network::run() {
  setupWifi();
  if (!portalActive_) {
    web_.begin();
  }

  for (;;) {
    const uint32_t now = millis();
    loopWifi(now);

    const bool wifiUp = WiFi.status() == WL_CONNECTED;
    const core::SystemStatus status = status_.get();
    mqtt_.loop(now, wifiUp, status);

    if (!portalActive_) {
      web_.loop();
      if (web_.takePortalRequest()) {
        portalRequested_ = true;
      }
    }

    publishInfo(now);

    if (now - lastDiagnosticsMs_ >= kDiagnosticsMs) {
      lastDiagnosticsMs_ = now;
      util::logf("Diagnose", "Freier Heap %u Bytes, Stack-Reserve Netzwerk-Task %u Bytes",
                 static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }

    vTaskDelay(pdMS_TO_TICKS(kLoopMs));  // auch den Watchdog des Idle-Tasks auf Core 0 bedienen
  }
}

void Network::setupWifi() {
  WiFi.setHostname(config::kHostname);  // muss vor WiFi.mode() stehen
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  // NTP läuft im Hintergrund und gleicht stündlich ab. Bis das WLAN steht, versucht es still weiter.
  configTzTime(util::kTimezone, config::kNtpServer1, config::kNtpServer2);

  wifiManager_ = new WiFiManager();
  wifiManager_->setConfigPortalBlocking(false);
  wifiManager_->setConfigPortalTimeout(config::kSetupPortalTimeoutS);
  wifiManager_->setTitle("Umkehrosmoseanlage");

  if (wifiManager_->getWiFiIsSaved()) {
    util::logf("WLAN", "Verbinde mit gespeichertem Netz");
    WiFi.begin();
  } else {
    util::logf("WLAN", "Keine Zugangsdaten gespeichert");
    startPortal();
  }
  lastReconnectMs_ = millis();
}

void Network::startPortal() {
  if (portalActive_) {
    return;
  }
  web_.stop();  // das Portal braucht Port 80
  wifiManager_->startConfigPortal(config::kSetupApName);
  portalActive_ = true;
  util::logf("WLAN", "Einrichtungsportal offen: WLAN \"%s\", http://192.168.4.1", config::kSetupApName);
}

void Network::loopWifi(uint32_t nowMs) {
  if (portalActive_) {
    wifiManager_->process();
    if (!wifiManager_->getConfigPortalActive()) {
      portalActive_ = false;
      util::logf("WLAN", "Einrichtungsportal geschlossen");
      WiFi.mode(WIFI_STA);
      if (WiFi.status() != WL_CONNECTED && wifiManager_->getWiFiIsSaved()) {
        WiFi.begin();
      }
      lastReconnectMs_ = nowMs;
      reconnectIntervalMs_ = kReconnectMinMs;
      web_.begin();
    }
    return;
  }

  if (portalRequested_.exchange(false)) {
    startPortal();
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasConnected_) {
      wifiWasConnected_ = true;
      reconnectIntervalMs_ = kReconnectMinMs;
      util::logf("WLAN", "Verbunden mit %s, IP %s, %d dBm", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                 static_cast<int>(WiFi.RSSI()));
    }
    if (!timeLogged_ && util::timeValid()) {
      timeLogged_ = true;
      util::logf("Zeit", "Uhrzeit per NTP gestellt");
    }
    return;
  }

  if (wifiWasConnected_) {
    wifiWasConnected_ = false;
    lastReconnectMs_ = nowMs;
    util::logf("WLAN", "Verbindung verloren");
  }

  // Der ESP32 versucht es selbst. Hängt er fest, hilft ein vollständiger Neuaufbau der Verbindung.
  if (nowMs - lastReconnectMs_ >= reconnectIntervalMs_) {
    lastReconnectMs_ = nowMs;
    if (wifiManager_->getWiFiIsSaved()) {
      util::logf("WLAN", "Neuer Verbindungsversuch");
      WiFi.disconnect();
      WiFi.begin();
    }
    reconnectIntervalMs_ *= 2;
    if (reconnectIntervalMs_ > kReconnectMaxMs) reconnectIntervalMs_ = kReconnectMaxMs;
  }
}

void Network::publishInfo(uint32_t nowMs) {
  if (nowMs - lastInfoMs_ < kInfoMs) {
    return;
  }
  lastInfoMs_ = nowMs;

  core::NetworkInfo info;
  info.wifiConnected = WiFi.status() == WL_CONNECTED;
  info.portalActive = portalActive_;
  if (info.wifiConnected) {
    info.rssi = static_cast<int8_t>(WiFi.RSSI());
    snprintf(info.ip, sizeof(info.ip), "%s", WiFi.localIP().toString().c_str());
    snprintf(info.ssid, sizeof(info.ssid), "%s", WiFi.SSID().c_str());
  }
  info.mqttConfigured = mqtt_.configured();
  info.mqttConnected = mqtt_.connected();
  info_.set(info);
}

}  // namespace net
