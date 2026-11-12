#pragma once
// Webpanel auf Port 80, läuft im Netzwerk-Task.
//   /              Status und Aktionen
//   /settings      Einstellungen (Spülplan, Dauern, Sicherheitslimits, Durchfluss, MQTT)
//   /api/status    Status als JSON
//   /mqtt          alte Adresse, leitet auf /settings um
// Änderungen gelten sofort, ein Neustart ist nicht nötig.

#include <WebServer.h>

#include "../app/shared.h"
#include "../core/status.h"

namespace net {

class WebPanel {
 public:
  WebPanel(app::RequestQueue& requests, app::SettingsHub& settings, app::Shared<core::SystemStatus>& status);

  void begin();
  void stop();
  void loop();

  // true, wenn im Webpanel "WLAN neu einrichten" gedrückt wurde
  bool takePortalRequest();

 private:
  void registerRoutes();
  void handleStatus();
  void handleSettingsForm();
  void handleSettingsSave();
  void handleAction();
  void handleApi();
  void sendPage(const char* title, const String& body);
  void redirect(const char* location);

  app::RequestQueue& requests_;
  app::SettingsHub& settings_;
  app::Shared<core::SystemStatus>& status_;
  WebServer server_;
  bool routesRegistered_ = false;
  bool running_ = false;
  bool portalRequested_ = false;
};

}  // namespace net
