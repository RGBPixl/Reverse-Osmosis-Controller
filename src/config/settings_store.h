#pragma once
// Laden und Speichern von Einstellungen und Laufzeitdaten im NVS.
// Einzige Stelle, die NVS-Schlüssel kennt.
//
// Die Schlüssel der alten Firmware bleiben gültig, damit die laufende Anlage ihre
// Einstellungen beim Umstieg behält und ein Zurückflashen von master möglich bleibt.

#include <stdint.h>

#include "settings.h"

namespace config {

// Lädt alle Einstellungen. Fehlende Werte bekommen den Standard, alte Werte werden übernommen.
Settings loadSettings();

// Speichert alle Einstellungen. Gibt false zurück, wenn der NVS nicht geöffnet werden konnte.
bool saveSettings(const Settings& settings);

// Werksreset: löscht alle Einstellungen inkl. MQTT-Zugang.
// Wassermengenzähler und WLAN-Zugangsdaten bleiben erhalten.
void eraseSettings();

// Gesamtzähler Wassermenge im Zulauf in Litern.
double loadTotalLiters();
bool saveTotalLiters(double liters);

// Zeitpunkt (Unix-Zeit) des letzten Starts mit gültiger Uhrzeit, 0 = unbekannt.
uint32_t loadLastBootEpoch();
bool saveLastBootEpoch(uint32_t epoch);

}  // namespace config
