#include "webpanel.h"
#include <Preferences.h>
#include <WiFi.h>

WebServer server(80);

void handleRoot() {
  server.send(200, "text/html", R"rawliteral(
    <html>
    <head><title>MQTT Setup</title></head>
    <body>
      <h2>MQTT Einstellungen</h2>
      <form action="/save" method="POST">
        Server: <input type="text" name="server"><br>
        Benutzer: <input type="text" name="user"><br>
        Passwort: <input type="password" name="pass"><br>
        <input type="submit" value="Speichern">
      </form>
    </body>
    </html>
  )rawliteral");
}

void handleSave() {
  if (server.hasArg("server") && server.hasArg("user") && server.hasArg("pass")) {
    Preferences prefs;
    prefs.begin("MQTT", false);
    prefs.putString("mqtt_server", server.arg("server"));
    prefs.putString("mqtt_user", server.arg("user"));
    prefs.putString("mqtt_pass", server.arg("pass"));
    prefs.end();
  }
  server.send(200, "text/html", "<html><body><h2>Gespeichert. Neustart...</h2></body></html>");
  delay(1000);
  ESP.restart();
}

void setupWebServer() {
  server.on("/mqtt", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("Webpanel gestartet unter: http://" + WiFi.localIP().toString() + "/mqtt");
}

void handleWebServer() {
  server.handleClient();
}