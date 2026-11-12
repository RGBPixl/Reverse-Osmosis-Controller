# Reverse-Osmosis-Controller

Firmware für die Steuerung einer Umkehrosmoseanlage mit einem ESP32 (uPesy WROOM).

Die Anlage regelt den Frischwasserzulauf über einen Druckschalter, spült System und Membran nach Zeitplan oder von
Hand, misst Temperaturen und Wassermenge und lässt sich am Gerät, im Browser und über Home Assistant bedienen. Sie
läuft auch komplett ohne WLAN.

## Funktionen

- **Druckregelung:** Fällt der Druck ab, öffnet der Zulauf; ist er wieder da, schließt er. Druckschalter entprellt,
  Ventil mindestens 10 s in einer Stellung.
- **Spülprogramme:**
  - Systemspülung: zu zwei festen Uhrzeiten, per Intervall oder von Hand.
  - Membranspülung: per Intervall oder von Hand.
  - Startspülung: nach dem Einschalten, wenn der letzte Start über 1 h her ist (ohne Uhrzeit immer).
- **Sicherheit:** Bleibt der Zulauf zu lange offen oder baut sich nach einer Entnahme kein Druck auf, schließen alle
  Ventile, bis der Fehler quittiert wird. Warnungen bei „Zulauf offen, aber kein Durchfluss“ und „Zulauf zu, aber
  Durchfluss“.
- **Service-Modus:** Automatik pausiert, Ventile einzeln von Hand schaltbar (am Gerät oder in Home Assistant).
- **Messwerte:** Temperatur vor und nach dem Filter, Wassermenge im Zulauf (übersteht Neustarts), Durchfluss in L/min.
- **Netzwerk:** WLAN mit automatischem Reconnect, NTP mit Sommer- und Winterzeit, MQTT mit Home-Assistant-Discovery,
  Webpanel. Fällt das Netz aus, läuft die Steuerung unverändert weiter.

## Hardware

| Teil | GPIO |
|---|---|
| Relais Frischwasser / Abwasser / Membran-Bypass | 12 / 27 / 26 |
| Druckschalter (HIGH = Druck da) | 35 |
| Durchflusssensor im Zulauf | 34 |
| 2× DS18B20 (Index 0 vor, Index 1 nach Filter) | 4 |
| LCD 16×2, I2C-Adresse 0x27 | 21 (SDA) / 22 (SCL) |
| Taster OK / Links / Rechts | 2 / 15 / 0 |
| LED-Ring, 8× WS2812B | 23 |
| RGB-Status-LED | 18 / 19 / 14 |
| reserviert: Container-Relais, Reserve-Relais 1/2, Schwimmer | 25, 33, 32, 36 |

Alle Pins stehen in [`src/config/pins.h`](src/config/pins.h). Nicht angeschlossene Teile und geplante Funktionen
beschreibt [`docs/spaetere-features.md`](docs/spaetere-features.md).

## Bedienung

### Display und Taster

Die Startseite zeigt den Zustand, die Uhrzeit, das WLAN-Symbol und je nach Zustand die Wassermenge, den Durchfluss,
die Restzeit einer Spülung oder den Fehlergrund.

| Taste | Startseite | im Menü |
|---|---|---|
| OK | Menü öffnen (bei Fehler direkt „Fehler quittieren“) | öffnen / ausführen |
| Rechts | – | weiter |
| Links | – | zurück |
| Links lang | – | zur Startseite |

Das Menü hat vier Kategorien: **Status** (Messwerte, Ventile, Uhrzeit), **Aktionen** (Spülungen, Service-Modus,
Ventile, Fehler quittieren), **Netzwerk** (WLAN, IP, MQTT, WLAN einrichten) und **System** (Zähler löschen,
LED-Test, Werksreset, Version). Zähler löschen und Werksreset brauchen ein zweites OK. Nach 30 s ohne Taste geht
es zurück zur Startseite.

### LEDs

| LED-Ring | | Status-LED | |
|---|---|---|---|
| grün | Bereit | grün | WLAN und MQTT verbunden |
| blau | Produziert | gelb | WLAN ok, MQTT nicht |
| blau rotierend | Spülung läuft | rot | kein WLAN |
| gelb | Service-Modus | blau blinkend | Einrichtungsportal offen |
| rot blinkend | Fehler | | |

Die Helligkeit der Status-LED ist im Webpanel einstellbar.

### Webpanel

Unter `http://<IP der Anlage>/` (IP im Menü unter Netzwerk):

- `/`: Status und Aktionen
- `/settings`: alle Einstellungen (siehe unten). Änderungen gelten sofort, ohne Neustart.
- `/api/status`: Status als JSON

### Home Assistant

Die Anlage meldet sich per MQTT-Discovery als Gerät „Umkehrosmoseanlage“. Den MQTT-Server stellst du im Webpanel ein.

- **Messwerte:** Temperatur IN/OUT, Wassermenge, Durchfluss, Druck, Zustand, Problem, Meldung, WLAN-Signal
- **Einstellungen:** System- und Membran-Spülintervall (Stunden), Spülzeit 1 und 2 (`HH:MM`, `--:--` = aus)
- **Bedienung:** Service-Modus, die drei Ventile (nur im Service-Modus wirksam), Buttons für Systemspülung,
  Membranspülung, Spülung abbrechen, Fehler quittieren und Zähler zurücksetzen

Alle Topics beginnen mit `reverse_osmosis/`, die Verfügbarkeit steht in `reverse_osmosis/status`.

### WLAN einrichten

Ohne gespeicherte Zugangsdaten oder über Menü → Netzwerk → „WLAN einrichten“ öffnet die Anlage das offene WLAN
**`RO-Setup`**. Darin unter `http://192.168.4.1` das Heimnetz auswählen. Das Portal schließt nach 5 min von selbst.

### Einstellungen und Standardwerte

| Einstellung | Standard |
|---|---|
| System-Spülintervall / Membran-Spülintervall | aus / aus |
| Feste Spülzeiten 1 und 2 | aus |
| Dauer je Schritt der Systemspülung | 60 s |
| Dauer Membranspülung | 300 s |
| Druckaufbau nach Entnahme max. | 300 s |
| Zulauf am Stück max. | 900 s |
| Entnahme-Schwelle (0 = Erkennung aus) | 0 L/min |
| Liter pro Impuls des Durchflusssensors | 0,00052 |
| Helligkeit Status-LED | 10 % |

Der Werksreset (Menü → System) setzt diese Werte und den MQTT-Zugang zurück. Wassermenge und WLAN bleiben erhalten.

## Entwicklung

### Aufbau

Die Steuerung (Arduino-`loop()`, Takt 10 ms) und das Netzwerk (eigener Task auf dem zweiten Kern) sind getrennt.
Relais schaltet ausschließlich die Ablaufsteuerung in `src/core/`. Menü, MQTT und Webpanel schicken ihr nur Befehle.
Nichts blockiert die Steuerung, auch kein hängender Verbindungsaufbau.

```
src/
  app/      verbindet alles; gemeinsame Daten zwischen den Tasks
  core/     Ablaufsteuerung, Spülprogramme, Zeitplan (ohne Arduino, getestet)
  hal/      Relais, Taster, Druckschalter, Durchfluss, Temperatur
  ui/       Startseite und Menü (getestet), LCD, LEDs
  net/      WLAN, MQTT, Webpanel
  config/   Pins, Einstellungen, NVS-Speicher, Versionen
  util/     Logging, Entprellung, Uhrzeit
test/       Unit-Tests für core, ui und net
```

Details und Entscheidungen: [`docs/refactoring-konzept.md`](docs/refactoring-konzept.md).

### Bauen und Flashen

Mit der PlatformIO-Erweiterung in VS Code (Build / Upload) oder im Terminal. `pio` gibt es im Terminal der
PlatformIO-Erweiterung, sonst unter `~/.platformio/penv/bin/pio`.

```bash
pio run
```

```bash
pio run -t upload
```

Plattform und Bibliotheken sind in [`platformio.ini`](platformio.ini) auf getestete Versionen festgelegt.
Gespeicherte Einstellungen, MQTT- und WLAN-Zugang sowie der Wasserzähler bleiben beim Flashen erhalten.

### Tests

Steuerung, Zeitplan, Menü, Einstellungen und MQTT-Auswertung laufen ohne Hardware auf dem Rechner:

```bash
pio test -e native
```

### Serielles Log

115200 Baud. Unter macOS startet schon das Öffnen und Schließen der seriellen Schnittstelle den ESP32 neu. Eine
Startspülung löst das nicht aus, solange der letzte Start unter einer Stunde her ist.

```bash
pio device monitor
```

### Sicherung und Wiederherstellung

Den kompletten Flash (4 MB, inklusive Einstellungen und Passwörter) sichern und zurückspielen. Den Port bei Bedarf
anpassen (`pio device list`). Sicherungen gehören in `backup/`, das von Git ausgeschlossen ist.

```bash
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port /dev/cu.usbserial-0001 --baud 460800 read_flash 0 0x400000 backup/flash-backup.bin
```

```bash
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port /dev/cu.usbserial-0001 --baud 460800 write_flash 0 backup/flash-backup.bin
```

## Lizenz

GNU General Public License v3.0, siehe [`LICENSE`](LICENSE).
