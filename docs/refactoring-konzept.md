# Refactoring-Konzept: Kernfeatures

Ziel: Die Firmware wird so neu aufgebaut, dass die Anlage zuverlässig läuft, **mit und ohne WLAN**, unabhängig davon,
in welcher Reihenfolge Menü, Home Assistant oder Zeitplan etwas auslösen.

Features, die bewusst nicht dazugehören, stehen in [spaetere-features.md](spaetere-features.md).

## 1. Umfang

### Angeschlossene Hardware (bleibt wie verdrahtet)

| Teil | GPIO |
|---|---|
| Relais Frischwasser / Abwasser / Membran-Bypass | 12 / 27 / 26 |
| Druckschalter | 35 |
| Durchflusssensor | 34 |
| 2× DS18B20 (vor / nach Filter) | 4 |
| LCD 16×2 (I2C, Adresse 0x27) | 21 / 22 |
| Taster OK / Links / Rechts | 2 / 15 / 0 |
| LED-Ring 8× WS2812B | 23 |
| RGB-Status-LED | 18 / 19 / 14 |

Container-Relais (25), Reserve-Relais (33, 32) und Schwimmer (36) werden beim Start sicher auf LOW gesetzt bzw.
ignoriert, sonst aber nicht benutzt.

### Kernfeatures

1. **Druckregelung:** Fällt der Druck ab, öffnet der Frischwasserzulauf; ist der Druck wieder da, schließt er.
   Neu sind Entprellung, Mindestschaltzeiten, die Erkennung des Entnahme-Endes und Sicherheitslimits.
2. **Spülprogramme:**
   - Startspülung beim Booten, wenn der letzte Neustart über 1 h her ist (offline immer).
   - Systemspülung: zu zwei festen Uhrzeiten, per Intervall und von Hand.
   - Membranspülung: per Intervall (neu fertig gebaut) und von Hand.
3. **Messwerte:**
   - Zwei Temperaturen.
   - Wassermenge im Zulauf: Gesamtzähler, der Neustarts übersteht und sich zurücksetzen lässt.
   - Durchfluss im Zulauf in L/min.
4. **Bedienung am Gerät:** LCD mit Menü, das echte Werte zeigt. LED-Ring für den Betriebszustand, RGB-LED für die
   Verbindung.
5. **Netzwerk:**
   - WLAN mit automatischem Reconnect; die Anlage läuft auch komplett offline.
   - NTP mit korrekter Sommer-/Winterzeit.
6. **Home Assistant:** MQTT mit Auto-Discovery. Die Relais sind nur im **Service-Modus** schaltbar.
7. **Webpanel:** Status und Einstellungen.
8. **Einstellungen:** an einer Stelle gespeichert, Werksreset funktioniert.

## 2. Architektur

### Grundprinzipien

1. **Nur die Ablaufsteuerung schaltet Relais.** Menü, MQTT, Webpanel und Zeitplan schicken ihr *Befehle*. Die
   Steuerung entscheidet, ob ein Befehl gerade erlaubt ist. Dadurch arbeitet nichts mehr gegeneinander.
2. **Nichts blockiert.** Keine `delay()`-Ketten mehr: Abläufe sind Zustandsmaschinen, die bei jedem Durchlauf auf die
   Uhr schauen. Spülungen sind jederzeit abbrechbar, und die Druckregelung läuft nie ins Leere.
3. **Nur das UI-Modul schreibt aufs LCD.** Keine Zugriffe von mehreren Tasks auf den I2C-Bus mehr.
4. **Das Netzwerk läuft getrennt.** WLAN, MQTT und Webpanel dürfen hängen, etwa bei einem Verbindungsaufbau, ohne dass
   die Steuerung davon etwas merkt.
5. **Die Steuerlogik ist hardwareunabhängig.** Dadurch lässt sie sich auf dem Mac mit Unit-Tests prüfen, ohne die Anlage
   anzufassen.

### Tasks

```
 Core 1: Arduino-loop()  (nie blockierend, ~10 ms Takt)        Core 0: Netzwerk-Task
 ┌───────────────────────────────────────────┐                  ┌──────────────────────────────┐
 │ Eingänge lesen (Taster, Druck, Durchfluss,│   Befehle        │ WLAN + Reconnect + Portal    │
 │ Temperatur)                               │ ◄─────────────── │ NTP / Zeitzone               │
 │ Ablaufsteuerung.tick()  → Relais          │   (Queue)        │ MQTT + HA-Discovery          │
 │ Zeitplan.tick()                           │                  │ Webpanel                     │
 │ UI: Display, Menü, LEDs                   │ ───────────────► │                              │
 └───────────────────────────────────────────┘ Status-Snapshot  └──────────────────────────────┘
                                                 (Mutex)
```

Es gibt genau zwei Berührungspunkte zwischen den Tasks:
- **Befehls-Queue** (Netzwerk → Steuerung), z. B. `StartSystemFlush`, `SetServiceMode(true)`, `SetRelay(1, on)`,
  `SetSetting(...)`, `ResetWaterCounter`, `AcknowledgeFault`.
- **Status-Snapshot** (Steuerung → Netzwerk): Eine Kopie aller Werte, die MQTT und Webpanel brauchen, geschützt durch
  einen Mutex.

Das Menü läuft im selben Task wie die Steuerung und benutzt dieselben Befehle.

### Zustände der Ablaufsteuerung

| Zustand | Bedeutung | Ventile |
|---|---|---|
| `Idle` | Bereit, Druck ist da | alle zu |
| `Producing` | Druck fehlt, Anlage produziert | Frischwasser auf |
| `Flushing` | Spülprogramm läuft: Start-, System- oder Membranspülung | laut Programm |
| `Service` | Automatik pausiert, Relais von Hand schaltbar | manuell |
| `Fault` | Sicherheitsabschaltung, wartet auf Quittierung | alle zu |

Regeln:
- Eine Spülanforderung während `Producing` unterbricht die Produktion. Danach geht es mit der Druckregelung weiter.
- Anforderungen während einer laufenden Spülung werden vorgemerkt (höchstens eine pro Programm), nicht verworfen.
- Fällige Spülungen im `Service`-Modus werden nach dem Service-Modus nachgeholt.
- `Fault` wird durch die Sicherheitslimits der Druckregelung ausgelöst (siehe unten). Später kommen der
  Overflow-Sensor und ähnliche Fehlerquellen hinzu. Quittieren geht über Menü, HA-Button oder Webpanel.
  Fehlt nach dem Quittieren weiterhin Druck, öffnet der Zulauf sofort wieder, und die Limits laufen von vorn.
- Den Service-Modus kann man im Fehlerzustand nicht einschalten, erst muss der Fehler quittiert sein. Auch im
  Service-Modus gilt das Zeitlimit für den offenen Zulauf.

### Spülprogramme als Tabellen

Programme sind Listen von Schritten (`welche Ventile offen`, `Dauer`) statt Code mit `delay()`:

| Programm | Schritte |
|---|---|
| Startspülung | 5 s Frischwasser → 10 s +Bypass → 10 s Frischwasser+Abwasser → 10 s Frischwasser+Bypass |
| Systemspülung | 1 s Frischwasser → 60 s +Bypass → 60 s Frischwasser+Abwasser → 60 s Frischwasser+Bypass |
| Membranspülung | **5 min** Frischwasser+Abwasser+Bypass (bisher 5 s) |

Die Dauern werden Einstellungen mit diesen Werten als Standard.

**Startspülung:** Läuft nur, wenn der letzte Neustart mehr als 1 h her ist. Das verhindert eine Spülung bei jedem
Neustart kurz hintereinander (z. B. beim Flashen). Dafür wird bei jedem Start mit gültiger Uhrzeit der Zeitpunkt im
NVS gespeichert. Ablauf beim Booten:
- Die Steuerung wartet bis zu 60 s auf eine gültige Uhrzeit. Die Druckregelung läuft in dieser Zeit schon normal.
- Uhrzeit da und letzter Neustart weniger als 1 h her: keine Startspülung.
- Uhrzeit da und letzter Neustart mehr als 1 h her: Startspülung.
- Keine Uhrzeit (offline) oder kein gespeicherter Zeitpunkt: Startspülung läuft immer.
- Die bisherige Wartezeit von 5 s mit geschlossenen Ventilen entfällt, weil ohnehin bis zu 60 s gewartet wird.

### Zeit und Offline-Betrieb

- **Zeitzone:** Die Zeitzone wird als `CET-1CEST,M3.5.0,M10.5.0/3` eingestellt. Sommer- und Winterzeit stimmen dann
  automatisch; bisher war die Anlage im Winter 1 h vor.
- **Ohne WLAN:**
  - Druckregelung, Intervallspülungen, manuelle Spülungen, Menü und LEDs laufen normal.
  - Feste Spülzeiten brauchen eine gültige Uhrzeit. Nach dem ersten NTP-Sync läuft die interne Uhr des ESP32 weiter,
    auch wenn das WLAN wegfällt; erst nach einem Neustart ohne WLAN fehlt die Zeit.
  - Ohne gültige Uhrzeit sind die festen Spülzeiten pausiert. Das Display zeigt dann `--:--`.
  - Für den kompletten Offline-Betrieb ist ein RTC-Modul vorgemerkt (siehe spätere Features).
- **WLAN:**
  - Die Verbindung wird im Hintergrund automatisch neu aufgebaut, ein Neustart ist nicht mehr nötig.
  - Das Einrichtungsportal öffnet nur, wenn noch keine WLAN-Daten gespeichert sind oder über den Menüpunkt
    „WLAN einrichten“. Es blockiert den Start nicht mehr.
- **Feste Spülzeiten:**
  - Gemerkt wird, an welchem Tag die Spülung zuletzt lief, statt um Mitternacht zurückzusetzen.
  - 00:00 ist eine gültige Zeit. „Aus“ wird ausdrücklich eingestellt (leer oder `--:--`).

### Druckregelung

- Der Druckschalter muss 2 s stabil sein, bevor er zählt.
- Das Ventil bleibt mindestens 10 s in einer Stellung, damit es nicht flattert.

**Entnahme erkennen:** Der Durchflusssensor sitzt im Zulauf. Während einer Entnahme fließt deutlich mehr als beim
Druckaufbau danach. Liegt der Durchfluss über der *Entnahme-Schwelle*, läuft gerade eine Entnahme. Fällt er darunter,
ist die Entnahme beendet, und der Druckaufbau beginnt.

**Sicherheitslimits** (bei Verletzung: `Fault`, alle Ventile zu):

| Limit | Standard | Schützt vor |
|---|---|---|
| Druckaufbau: nach Ende der Entnahme muss der Druck da sein | 5 min | Leck, Druckschalter defekt |
| Zulauf am Stück offen, egal ob Entnahme oder nicht | 15 min (längste Entnahme ca. 10 min + 5 min) | Leck, das wie eine Dauerentnahme aussieht |

**Plausibilitätswarnungen** (nur Meldung auf LCD und in HA, keine Abschaltung):
- Zulauf offen, aber 60 s lang kein Durchfluss → „Kein Zulauf“ (Wasserhahn zu, Ventil klemmt).
- Zulauf zu, aber Durchfluss → „Ventil undicht“.

Die Entnahme-Schwelle ist noch unbekannt und wird an der Anlage ermittelt: Der Durchfluss in L/min wird an HA
gemeldet, sodass sich typische Werte für Entnahme und Druckaufbau im Verlauf ablesen lassen. Bis dahin greift nur das
15-min-Limit. Alle Werte sind über das Webpanel einstellbar.

### Wassermenge

- Gezählt wird der **Zulauf** (Wasser, das in die Anlage geht), nicht das Reinwasser.
- Der Impulszähler im Interrupt wird sauber `volatile`/atomar.
- Der Gesamtzähler wird beim Start geladen. Gespeichert wird alle 10 L oder 30 min sowie nach jeder Spülung.
- Ein Reset über Menü oder HA setzt den Zähler wirklich auf 0.
- Der Umrechnungsfaktor (bisher 0,00052 L/Impuls) wird eine Einstellung.

### Bedienung am Gerät

- **Taster:** werden gepollt und entprellt, statt über Interrupts ohne Entprellung.
- **Display:**
  - Neu gezeichnet werden nur Zeilen, die sich geändert haben. Das verhindert Flackern und Müllzeichen.
  - Die Startseite zeigt Zustand, Uhrzeit, WLAN-Symbol und Wassermenge.
- **Menü:** wird als Baum definiert, nicht über magische Indizes. Links = zurück, Rechts = weiter, OK = auswählen,
  Links lang = Startseite. Nach 30 s ohne Eingabe geht es zurück zur Startseite. Bei einem Fehler führt OK auf der
  Startseite direkt zu „Fehler quittieren“.

```
Status        → Temperaturen, Wassermenge, Durchfluss, Druckschalter, Ventile, Uhrzeit
Aktionen      → Fehler quittieren*, Systemspülung, Membranspülung, Spülung stoppen*, Service-Modus,
                Ventil Zulauf / Abwasser / Bypass*
Netzwerk      → IP + Signal, MQTT-Status, WLAN einrichten (Phase 4)
System        → Zähler löschen, LED-Test, Werksreset, Version

* nur sichtbar, wenn es passt: Fehler aktiv, Spülung läuft, Service-Modus an
```

Kritische Aktionen (Werksreset, Zähler zurücksetzen) brauchen eine Bestätigung: „OK zum Bestätigen“, mit Timeout.

**LEDs**

| LED-Ring | Bedeutung |
|---|---|
| grün, ruhig | Bereit |
| blau, ruhig | Produziert (Zulauf offen) |
| blau, rotierend | Spülung läuft |
| gelb | Service-Modus |
| rot, blinkend | Fehler |

| RGB-Status-LED | Bedeutung |
|---|---|
| grün | WLAN + MQTT verbunden |
| gelb | WLAN ok, MQTT nicht |
| rot | kein WLAN |
| blau, blinkend | Einrichtungsportal offen |

### Home Assistant

Die bestehenden Topics (`reverse_osmosis/...`) und `unique_id`s bleiben erhalten, damit Entitäten und Verlauf in HA
nicht verloren gehen.

| Entität | Änderung |
|---|---|
| Temperatur IN/OUT, Wassermenge, Zähler zurücksetzen | bleiben; der Reset funktioniert jetzt wirklich |
| System-Spülintervall, Spülzeit 1/2 | bleiben; neue Werte werden korrekt zurückgemeldet |
| Relais Frischwasser / Abwasser / Membran-Bypass | bleiben als Schalter; Befehle werden nur im Service-Modus angenommen, sonst springt der Schalter zurück |
| **neu:** Service-Modus | Schalter |
| **neu:** Membran-Spülintervall | Zahl (Stunden) |
| **neu:** Systemspülung / Membranspülung starten, Fehler quittieren | Buttons |
| **neu:** Zustand | Sensor (Bereit / Produziert / Spült / Service / Fehler) |
| **neu:** Fehler | binary_sensor `problem` + Text mit Ursache bzw. Warnung |
| **neu:** Durchfluss | Sensor in L/min (Zulauf) |
| Schwimmer, Container, Reserve 1/2 | werden aus HA entfernt (leere Discovery-Nachricht), siehe spätere Features |

Der MQTT-Reconnect blockiert die Steuerung nicht mehr, und nach jedem Reconnect werden Discovery und Zustände erneut
gesendet.

### Webpanel

- Startseite mit Status: Zustand, Messwerte, Verbindung.
- Einstellungen: MQTT, Spülzeiten, Intervalle, Spüldauern, Sicherheitslimits, Entnahme-Schwelle, Durchflussfaktor.
- Änderungen gelten sofort. Ein Neustart nach dem Speichern ist nicht mehr nötig.

### Einstellungen

- Ein `Settings`-Struct mit Standardwerten, Laden und Speichern an genau einer Stelle (`config/settings.cpp`).
- Die bisherigen NVS-Schlüssel (`iFS`, `iFM`, `fT1H`, `fT1M`, `fT2H`, `fT2M`, `mqtt_server`, `mqtt_user`,
  `mqtt_pass`, `flowLiters`) bleiben gültig. Die laufende Anlage behält ihre Einstellungen also beim ersten Flashen.
- Der Werksreset setzt alles auf Standard; WLAN-Daten werden nur auf ausdrücklichen Wunsch gelöscht.

## 3. Dateistruktur

```
src/
  main.cpp                 nur setup()/loop(): Module verbinden
  config/pins.h            alle Pins an einer Stelle (auch reservierte)
  config/settings.*        Einstellungen, Standardwerte, NVS
  hal/relays.*             Ventile schalten + Logging
  hal/inputs.*             Taster und Druckschalter, entprellt
  hal/flow_meter.*         Impulszähler, Liter, L/min
  hal/temperature.*        DS18B20, nicht blockierend
  core/controller.*        Zustandsmaschine, einziger Relais-Schalter
  core/programs.h          Spülprogramme als Tabellen
  core/scheduler.*         feste Spülzeiten und Intervalle
  core/commands.h          Befehle an die Steuerung
  core/status.h            Status-Snapshot
  net/network_task.*       WLAN, Reconnect, Portal, NTP
  net/mqtt.*               MQTT + HA-Discovery
  net/web.*                Webpanel
  ui/display.*             LCD (einziger Schreiber)
  ui/menu.*                Menübaum
  ui/leds.*                LED-Ring + RGB-Status-LED
test/
  test_controller/         Unit-Tests der Ablaufsteuerung (laufen auf dem Mac)
```

`core/` hängt nicht von Arduino ab. Die Steuerung bekommt Zeit und Eingänge übergeben und liefert die gewünschten
Ventilzustände zurück. Dafür bekommt `platformio.ini` eine zusätzliche Umgebung `native` für die Tests.

## 4. Vorgehen

Weil die Anlage in Betrieb ist, läuft das Refactoring auf einem eigenen Branch. `master` bleibt als lauffähige
Rückfalloption. Vor dem ersten Flashen wird die aktuelle Firmware als `.bin` gesichert.

| Phase | Inhalt | Prüfung |
|---|---|---|
| 1 | Grundgerüst: `pins.h`, Einstellungen, HAL (Relais, Eingänge, Durchfluss, Temperatur) | Build |
| 2 | Ablaufsteuerung, Spülprogramme, Zeitplan, Druckregelung, Fehlerzustand | Build + Unit-Tests |
| 3 | UI: Display, Menü, LEDs. Danach ist die Anlage offline voll bedienbar | Build, erster Test an der Anlage |
| 4 | Netzwerk-Task: WLAN-Reconnect, NTP, MQTT/HA, Webpanel | Build, Test an der Anlage mit HA |
| 5 | Alten Code entfernen, README schreiben | Build |

## 5. Festgelegte Entscheidungen

| Thema | Entscheidung |
|---|---|
| Membranspülung | 5 min (bisher 5 s) |
| Systemspülung | Schritte bleiben bei 60 s |
| Druckaufbau nach Entnahme | höchstens 5 min, sonst Fehler |
| Zulauf am Stück | höchstens 15 min, sonst Fehler |
| Durchflusssensor | sitzt im Zulauf; Entnahme ist am höheren Durchfluss erkennbar |
| LED-Bedeutungen | wie oben vorgeschlagen |
| Startspülung | nur wenn der letzte Neustart über 1 h her ist, offline immer |
| HA-Relais | nur im Service-Modus schaltbar |
| Membranspülung per Intervall | gehört zum Kern |

**Noch offen:** Die Entnahme-Schwelle in L/min wird nach dem ersten Betrieb aus den HA-Werten bestimmt.
