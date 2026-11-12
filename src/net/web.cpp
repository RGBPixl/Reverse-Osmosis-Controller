#include "web.h"

#include "../config/network.h"
#include "../config/version.h"
#include "../util/clock.h"
#include "../util/log.h"
#include "protocol.h"

namespace net {

namespace {

String escapeHtml(const char* text) {
  String out;
  for (const char* p = text; *p != '\0'; ++p) {
    switch (*p) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += *p;
    }
  }
  return out;
}

String escapeJson(const char* text) {
  String out;
  for (const char* p = text; *p != '\0'; ++p) {
    if (*p == '"' || *p == '\\') out += '\\';
    if (static_cast<unsigned char>(*p) >= 0x20) out += *p;
  }
  return out;
}

String row(const char* label, const String& value) {
  return String("<tr><th>") + label + "</th><td>" + value + "</td></tr>";
}

String number(double value, unsigned decimals) { return String(value, decimals); }

String temperature(const core::Measurements& m, uint8_t index) {
  return m.tempValid[index] ? number(m.tempC[index], 1) + " &deg;C" : String("kein Sensor");
}

String valveText(core::ValveMask valves) {
  String out;
  out += String("Zulauf ") + (core::isOpen(valves, core::Valve::FreshWater) ? "auf" : "zu");
  out += String(", Abwasser ") + (core::isOpen(valves, core::Valve::WasteWater) ? "auf" : "zu");
  out += String(", Bypass ") + (core::isOpen(valves, core::Valve::MembraneBypass) ? "auf" : "zu");
  return out;
}

String clockText(const core::WallClock& clock) {
  if (!clock.valid) return "nicht gestellt";
  const util::CivilDate d = util::civilFromDays(static_cast<int32_t>(clock.day));
  char text[24];
  snprintf(text, sizeof(text), "%02u.%02u.%04ld %02u:%02u", d.day, d.month, static_cast<long>(d.year),
           clock.minuteOfDay / 60u, clock.minuteOfDay % 60u);
  return text;
}

String button(const char* action, const char* label, const char* confirm = nullptr) {
  String out = "<button name=do value=";
  out += action;
  if (confirm != nullptr) {
    out += " onclick=\"return confirm('";
    out += confirm;
    out += "')\"";
  }
  out += ">";
  out += label;
  out += "</button> ";
  return out;
}

String numberInput(const char* name, const char* label, double value, double min, double max, const char* step,
                   const char* unit, unsigned decimals = 0) {
  String out = "<label>";
  out += label;
  out += "<span><input type=number name=";
  out += name;
  out += " value=\"" + number(value, decimals) + "\" min=\"" + number(min, decimals) + "\" max=\"" +
         number(max, decimals) + "\" step=\"" + step + "\"> " + unit + "</span></label>";
  return out;
}

String flushTimeInput(const char* prefix, const char* label, const config::FlushTime& t) {
  char time[8];
  snprintf(time, sizeof(time), "%02u:%02u", static_cast<unsigned>(t.hour), static_cast<unsigned>(t.minute));
  String out = "<label>";
  out += label;
  out += "<span><input type=checkbox name=";
  out += prefix;
  out += "_on";
  out += t.enabled ? " checked" : "";
  out += "> aktiv <input type=time name=";
  out += prefix;
  out += " value=";
  out += time;
  out += "></span></label>";
  return out;
}

uint8_t toU8(long v) { return v < 0 ? 0 : (v > 255 ? 255 : static_cast<uint8_t>(v)); }
uint16_t toU16(long v) { return v < 0 ? 0 : (v > 65535 ? 65535 : static_cast<uint16_t>(v)); }

const char kStyle[] =
    "body{font-family:system-ui,sans-serif;max-width:640px;margin:0 auto;padding:16px;color:#222}"
    "h1{font-size:1.4em}h2{font-size:1.1em;margin-top:1.5em}"
    "table{border-collapse:collapse;width:100%}th,td{text-align:left;padding:6px 4px;border-bottom:1px solid #ddd}"
    "th{font-weight:normal;color:#666;width:45%}button{padding:8px 12px;margin:4px 4px 4px 0}"
    "fieldset{border:1px solid #ddd;margin:12px 0}label{display:flex;justify-content:space-between;"
    "align-items:center;gap:8px;margin:6px 0}input[type=number],input[type=text],input[type=password]"
    "{width:9em}.warn{color:#b00;font-weight:bold}.ok{color:#080}a{color:#06c}";

}  // namespace

WebPanel::WebPanel(app::RequestQueue& requests, app::SettingsHub& settings, app::Shared<core::SystemStatus>& status)
    : requests_(requests), settings_(settings), status_(status), server_(80) {}

void WebPanel::begin() {
  if (!routesRegistered_) {
    registerRoutes();
    routesRegistered_ = true;
  }
  if (!running_) {
    server_.begin();
    running_ = true;
    util::logf("Web", "Webpanel gestartet");
  }
}

void WebPanel::stop() {
  if (running_) {
    server_.stop();
    running_ = false;
  }
}

void WebPanel::loop() {
  if (running_) server_.handleClient();
}

bool WebPanel::takePortalRequest() {
  const bool requested = portalRequested_;
  portalRequested_ = false;
  return requested;
}

void WebPanel::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/action", HTTP_POST, [this]() { handleAction(); });
  server_.on("/settings", HTTP_GET, [this]() { handleSettingsForm(); });
  server_.on("/settings", HTTP_POST, [this]() { handleSettingsSave(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleApi(); });
  server_.on("/mqtt", HTTP_GET, [this]() { redirect("/settings"); });
  server_.onNotFound([this]() { server_.send(404, "text/plain", "Nicht gefunden"); });
}

void WebPanel::sendPage(const char* title, const String& body) {
  String page;
  page.reserve(body.length() + 1200);
  page += "<!doctype html><html lang=de><head><meta charset=utf-8>"
          "<meta name=viewport content=\"width=device-width,initial-scale=1\"><title>";
  page += title;
  page += "</title><style>";
  page += kStyle;
  page += "</style></head><body>";
  page += body;
  page += "</body></html>";
  server_.send(200, "text/html; charset=utf-8", page);
}

void WebPanel::redirect(const char* location) {
  server_.sendHeader("Location", location);
  server_.send(303, "text/plain", "");
}

void WebPanel::handleStatus() {
  const core::SystemStatus s = status_.get();
  const core::ControllerStatus& c = s.controller;
  const core::Measurements& m = s.measurements;
  char text[96];

  String body;
  body.reserve(3000);
  body += "<meta http-equiv=refresh content=10><h1>Umkehrosmoseanlage</h1><table>";

  stateText(c, text, sizeof(text));
  String state = text;
  if (c.mode == core::Mode::Flushing) {
    char remaining[48];
    snprintf(remaining, sizeof(remaining), " (Schritt %u/%u, noch %lu:%02lu)", static_cast<unsigned>(c.step + 1),
             static_cast<unsigned>(c.stepCount), static_cast<unsigned long>(c.programRemainingMs / 60000),
             static_cast<unsigned long>((c.programRemainingMs / 1000) % 60));
    state += remaining;
  }
  body += row("Zustand", state);

  problemText(c, text, sizeof(text));
  body += row("Meldung", String(hasProblem(c) ? "<span class=warn>" : "<span class=ok>") + text + "</span>");
  body += row("Temperatur vor Filter", temperature(m, 0));
  body += row("Temperatur nach Filter", temperature(m, 1));
  body += row("Wassermenge (Zulauf)", number(m.totalLiters, 2) + " L");
  body += row("Durchfluss", number(m.flowLpm, 2) + " L/min");
  body += row("Druckschalter", m.pressurized ? "Druck da" : "kein Druck");
  body += row("Ventile", valveText(c.valves));
  body += row("Uhrzeit", clockText(s.clock));
  // int8_t ausdrücklich als Zahl, String(int8_t) wäre ein einzelnes Zeichen
  const int rssi = s.network.rssi;
  body += row("WLAN", s.network.wifiConnected
                          ? escapeHtml(s.network.ssid) + " (" + String(rssi) + " dBm, " + s.network.ip + ")"
                          : String("nicht verbunden"));
  body += row("MQTT", !s.network.mqttConfigured ? "kein Server eingestellt"
                                                : (s.network.mqttConnected ? "verbunden" : "getrennt"));
  body += row("Firmware", config::kFirmwareVersion);
  body += "</table>";

  body += "<h2>Aktionen</h2><form method=post action=/action>";
  if (c.mode == core::Mode::Fault) body += button("ack_fault", "Fehler quittieren");
  body += button("flush_system", "Systemspülung");
  body += button("flush_membrane", "Membranspülung");
  if (c.mode == core::Mode::Flushing) body += button("abort_flush", "Spülung abbrechen");
  body += c.mode == core::Mode::Service ? button("service_off", "Service-Modus aus") : button("service_on", "Service-Modus an");
  body += button("reset_counter", "Zähler zurücksetzen", "Wasserzähler wirklich auf 0 setzen?");
  body += button("wifi_portal", "WLAN neu einrichten",
                 "Einrichtungsportal starten? Das Webpanel ist dann über das WLAN RO-Setup erreichbar.");
  body += "</form><p><a href=/settings>Einstellungen</a> &middot; <a href=/api/status>JSON</a></p>";

  sendPage("Umkehrosmoseanlage", body);
}

void WebPanel::handleAction() {
  const String action = server_.arg("do");
  util::logf("Web", "Aktion: %s", action.c_str());

  if (action == "reset_counter") {
    requests_.push(app::Request::resetCounter());
  } else if (action == "wifi_portal") {
    portalRequested_ = true;
  } else {
    core::Command command;
    if (action == "flush_system") {
      command = core::Command::startFlush(core::Program::System);
    } else if (action == "flush_membrane") {
      command = core::Command::startFlush(core::Program::Membrane);
    } else if (action == "abort_flush") {
      command = core::Command::abortFlush();
    } else if (action == "ack_fault") {
      command = core::Command::acknowledgeFault();
    } else if (action == "service_on" || action == "service_off") {
      command = core::Command::serviceMode(action == "service_on");
    } else {
      server_.send(400, "text/plain", "Unbekannte Aktion");
      return;
    }
    requests_.push(app::Request::controller(command));
  }
  redirect("/");
}

void WebPanel::handleSettingsForm() {
  const config::Settings s = settings_.get();
  String body;
  body.reserve(4000);
  body += "<h1>Einstellungen</h1>";
  if (server_.hasArg("saved")) body += "<p class=ok>Gespeichert.</p>";
  body += "<form method=post action=/settings>";

  body += "<fieldset><legend>Systemspülung</legend>";
  body += numberInput("sys_interval", "Intervall (0 = aus)", s.systemFlushIntervalH, 0, config::kMaxFlushIntervalH, "1", "h");
  body += flushTimeInput("t1", "Feste Zeit 1", s.flushTimes[0]);
  body += flushTimeInput("t2", "Feste Zeit 2", s.flushTimes[1]);
  body += numberInput("sys_step", "Dauer je Schritt", s.systemFlushStepS, config::kMinSystemFlushStepS,
                      config::kMaxSystemFlushStepS, "1", "s");
  body += "</fieldset>";

  body += "<fieldset><legend>Membranspülung</legend>";
  body += numberInput("mem_interval", "Intervall (0 = aus)", s.membraneFlushIntervalH, 0, config::kMaxFlushIntervalH, "1",
                      "h");
  body += numberInput("mem_flush", "Dauer", s.membraneFlushS, config::kMinMembraneFlushS, config::kMaxMembraneFlushS, "1",
                      "s");
  body += "</fieldset>";

  body += "<fieldset><legend>Sicherheit</legend>";
  body += numberInput("p_build", "Druckaufbau nach Entnahme max.", s.pressureBuildMaxS, config::kMinPressureBuildS,
                      config::kMaxPressureBuildS, "1", "s");
  body += numberInput("inlet_max", "Zulauf am Stück max.", s.inletMaxOpenS, config::kMinInletOpenS,
                      config::kMaxInletOpenS, "1", "s");
  body += numberInput("draw_lpm", "Entnahme ab (0 = Erkennung aus)", s.drawThresholdLpm, 0,
                      config::kMaxDrawThresholdLpm, "0.01", "L/min", 2);
  body += "</fieldset>";

  body += "<fieldset><legend>Durchflusssensor</legend>";
  body += numberInput("l_per_pulse", "Liter pro Impuls", s.litersPerPulse, config::kMinLitersPerPulse,
                      config::kMaxLitersPerPulse, "any", "L", 6);
  body += "</fieldset>";

  body += "<fieldset><legend>Anzeige</legend>";
  body += numberInput("led_pct", "Helligkeit Status-LED (0 = aus)", s.statusLedPercent, 0,
                      config::kMaxStatusLedPercent, "1", "%");
  body += "</fieldset>";

  body += "<fieldset><legend>MQTT</legend>";
  body += "<label>Server<span><input type=text name=mqtt_server value=\"" + escapeHtml(s.mqttServer) +
          "\" placeholder=\"leer = aus\"></span></label>";
  body += numberInput("mqtt_port", "Port", s.mqttPort, 1, 65535, "1", "");
  body += "<label>Benutzer<span><input type=text name=mqtt_user value=\"" + escapeHtml(s.mqttUser) +
          "\"></span></label>";
  body += "<label>Passwort<span><input type=password name=mqtt_pass placeholder=\"";
  body += s.mqttPassword[0] != '\0' ? "unverändert" : "kein Passwort";
  body += "\"></span></label>";
  body += "<label>Passwort löschen<span><input type=checkbox name=mqtt_pass_clear></span></label>";
  body += "</fieldset>";

  body += "<button>Speichern</button></form><p><a href=/>Zurück zum Status</a></p>";
  sendPage("Einstellungen", body);
}

void WebPanel::handleSettingsSave() {
  config::Settings s = settings_.get();

  auto text = [this](const char* name, String& value) -> bool {
    if (!server_.hasArg(name)) return false;
    value = server_.arg(name);
    value.trim();
    return true;
  };
  auto integer = [&text](const char* name, long& value) -> bool {
    String raw;
    if (!text(name, raw) || raw.isEmpty()) return false;
    value = raw.toInt();
    return true;
  };
  auto decimal = [&text](const char* name, float& value) -> bool {
    String raw;
    if (!text(name, raw) || raw.isEmpty()) return false;
    raw.replace(',', '.');
    value = raw.toFloat();
    return true;
  };

  long v = 0;
  float f = 0.0f;
  String raw;
  if (integer("sys_interval", v)) s.systemFlushIntervalH = toU8(v);
  if (integer("mem_interval", v)) s.membraneFlushIntervalH = toU8(v);
  if (integer("sys_step", v)) s.systemFlushStepS = toU16(v);
  if (integer("mem_flush", v)) s.membraneFlushS = toU16(v);
  if (integer("p_build", v)) s.pressureBuildMaxS = toU16(v);
  if (integer("inlet_max", v)) s.inletMaxOpenS = toU16(v);
  if (decimal("draw_lpm", f)) s.drawThresholdLpm = f;
  if (decimal("l_per_pulse", f)) s.litersPerPulse = f;
  if (integer("led_pct", v)) s.statusLedPercent = toU8(v);

  const char* const prefixes[] = {"t1", "t2"};
  for (size_t i = 0; i < config::kFlushTimeCount; ++i) {
    config::FlushTime t = s.flushTimes[i];
    if (text(prefixes[i], raw) && !raw.isEmpty()) {
      parseFlushTime(raw.c_str(), t);
    }
    t.enabled = server_.hasArg(String(prefixes[i]) + "_on");
    s.flushTimes[i] = t;
  }

  if (text("mqtt_server", raw)) snprintf(s.mqttServer, sizeof(s.mqttServer), "%s", raw.c_str());
  if (integer("mqtt_port", v)) s.mqttPort = toU16(v);
  if (text("mqtt_user", raw)) snprintf(s.mqttUser, sizeof(s.mqttUser), "%s", raw.c_str());
  if (server_.hasArg("mqtt_pass_clear")) {
    s.mqttPassword[0] = '\0';
  } else if (text("mqtt_pass", raw) && !raw.isEmpty()) {
    snprintf(s.mqttPassword, sizeof(s.mqttPassword), "%s", raw.c_str());
  }

  settings_.modify([&s](config::Settings& target) { target = s; });
  util::logf("Web", "Einstellungen geaendert");
  redirect("/settings?saved=1");
}

void WebPanel::handleApi() {
  const core::SystemStatus s = status_.get();
  const core::ControllerStatus& c = s.controller;
  const core::Measurements& m = s.measurements;
  char state[64];
  char problem[96];
  stateText(c, state, sizeof(state));
  problemText(c, problem, sizeof(problem));

  String json;
  json.reserve(700);
  json += "{\"state\":\"" + escapeJson(state) + "\"";
  json += ",\"problem\":" + String(hasProblem(c) ? "true" : "false");
  json += ",\"message\":\"" + escapeJson(problem) + "\"";
  json += ",\"temp1\":" + (m.tempValid[0] ? number(m.tempC[0], 2) : String("null"));
  json += ",\"temp2\":" + (m.tempValid[1] ? number(m.tempC[1], 2) : String("null"));
  json += ",\"liters\":" + number(m.totalLiters, 2);
  json += ",\"flow_lpm\":" + number(m.flowLpm, 2);
  json += ",\"pressure\":" + String(m.pressurized ? "true" : "false");
  json += ",\"valves\":{\"fresh\":" + String(core::isOpen(c.valves, core::Valve::FreshWater) ? "true" : "false");
  json += ",\"waste\":" + String(core::isOpen(c.valves, core::Valve::WasteWater) ? "true" : "false");
  json += ",\"bypass\":" + String(core::isOpen(c.valves, core::Valve::MembraneBypass) ? "true" : "false") + "}";
  json += ",\"program_remaining_s\":" + String(c.mode == core::Mode::Flushing ? c.programRemainingMs / 1000 : 0);
  json += ",\"inlet_open_s\":" + String(c.inletOpenMs / 1000);
  json += ",\"clock_valid\":" + String(s.clock.valid ? "true" : "false");
  json += ",\"wifi\":{\"connected\":" + String(s.network.wifiConnected ? "true" : "false");
  json += ",\"rssi\":" + String(static_cast<int>(s.network.rssi)) + ",\"ip\":\"" + escapeJson(s.network.ip) + "\"}";
  json += ",\"mqtt\":{\"configured\":" + String(s.network.mqttConfigured ? "true" : "false");
  json += ",\"connected\":" + String(s.network.mqttConnected ? "true" : "false") + "}";
  json += ",\"version\":\"" + String(config::kFirmwareVersion) + "\"}";
  server_.send(200, "application/json", json);
}

}  // namespace net
