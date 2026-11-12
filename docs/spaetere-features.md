# Spätere Features

Diese Features und Hardwareteile sind aktuell **nicht angeschlossen oder nicht nötig**. Sie sind bewusst aus dem
Refactoring der Kernfeatures (siehe [refactoring-konzept.md](refactoring-konzept.md)) herausgenommen und hier
festgehalten, damit sie später sauber eingebaut werden können.

Angaben zum bisherigen Code beziehen sich auf den Stand **vor dem Refactoring (Commit `07953e4`)**.

## Übersicht

| Feature | Hardware | Pin | Stand im alten Code |
|---|---|---|---|
| Kanister füllen | Container-Relais, Schwimmer | 25, 36 | funktioniert grundsätzlich, aber ohne Schutz |
| Desinfektion | unklar | – | nur Menüeintrag |
| Overflow-/Lecksensor | Sensor fehlt | – | nur Menüeintrag mit festem Text |
| Reserve-Relais 1/2 | Relaiskanäle | 33, 32 | ohne Funktion, in HA schaltbar |
| Dummy-Sensoren 5/6 | – | – | nur Menüeinträge mit festem Text |

## Kanister füllen

**Idee:** Ein Kanister wird über das Container-Relais befüllt, bis der Schwimmer meldet, dass er voll ist.

**Bisheriger Stand**
- Start über das Menü (`Funktionen → Kanister füllen`). Der Spül-Task schaltet das Container-Relais ein und wartet,
  bis der Schwimmer HIGH meldet.
- Kein Timeout und keine Mengenbegrenzung: Meldet der Schwimmer nie, läuft der Kanister über.
- Während des Füllens ist der Spül-Task komplett blockiert (keine Druckregelung, keine Spülungen).
- In `mqtt_handler.cpp` ist der Schwimmer fälschlich als GPIO 25 definiert (das ist das Container-Relais).
  HA zeigt unter „Schwimmer“ deshalb den Relaiszustand.
- Über HA lässt sich das Container-Relais direkt einschalten, am Schwimmer vorbei.

**Für den späteren Einbau**
- Als eigenes Programm in die Ablaufsteuerung (`core/`) einbauen, wie die Spülprogramme. Kein eigener Task, keine
  Warteschleife.
- Pflicht-Abbruchbedingungen: Schwimmer meldet voll, **maximale Laufzeit** und **maximale Menge** über den
  Durchflusssensor (falls der Sensor im richtigen Strang sitzt).
- Schwimmer entprellen. GPIO 36 ist ein reiner Eingang **ohne internen Pull-up/-down**, ein offener Eingang flattert.
  Deshalb braucht es einen externen Widerstand.
- Das Container-Relais darf nur über das Programm schalten, auch im Service-Modus nur mit Schwimmer-Überwachung.
- HA: die Entität `ro_float` (binary_sensor) wieder anlegen, dazu einen Button „Kanister füllen“.

## Desinfektion

**Bisheriger Stand:** Es gibt nur den Menüeintrag `Funktionen → Desinfektion`, dahinter keine Funktion.

**Vor dem Einbau zu klären**
- Wie läuft eine Desinfektion bei dieser Anlage ab? Wird ein Mittel dosiert (Pumpe → Reserve-Relais?) oder von Hand
  eingefüllt? Welche Ventilstellungen, wie lange einwirken, wie lange nachspülen?
- Gibt es Schritte, bei denen der Nutzer etwas tun muss? Dann braucht der Ablauf Bestätigungen am Display
  („Mittel eingefüllt? OK drücken“).

**Für den späteren Einbau:** als Programm mit Schritten in der Ablaufsteuerung, so wie die Spülprogramme.
Gegebenenfalls mit Warteschritten auf Bestätigung.

## Overflow-/Lecksensor

**Bisheriger Stand:** Nur die Menüseite `Sonst. Sensoren → Overflow Sensor` mit festem Text „offen“. Kein Pin, kein Code.

**Für den späteren Einbau**
- Freier Eingang, z. B. GPIO 13, 16 oder 17 (mit internem Pull-up) oder GPIO 39 (nur Eingang, externer Widerstand
  nötig).
- Höchste Priorität: Meldet der Sensor, geht die Steuerung sofort in den Fehlerzustand. Alle Ventile zu, Meldung auf
  LCD, LED und in HA. Weiter geht es erst nach Quittierung.
- Der Fehlerzustand ist im Refactoring bereits vorgesehen. Der Sensor muss nur als weitere Fehlerquelle eingehängt
  werden.
- HA: binary_sensor mit `device_class: moisture`.

## Reserve-Relais 1/2

**Bisheriger Stand:** GPIO 33 und 32 werden beim Start auf LOW gesetzt und sind in HA als Schalter „Reserve 1/2“
sichtbar, haben aber keine Funktion.

**Für den späteren Einbau:** Die Pins bleiben in `config/pins.h` reserviert. Erst mit einer konkreten Aufgabe
(z. B. Dosierpumpe für die Desinfektion) wieder in die Ablaufsteuerung und HA aufnehmen.

## Dummy-Sensoren 5/6

Die Menüseiten „Dummy Sensor 1/2“ und „Dummy R5/R6“ haben keine Bedeutung und fallen im Refactoring weg.
Kommt ein neuer Sensor dazu, bekommt er eine eigene, benannte Menüseite.

## Weitere Ideen

Diese Ideen sind nicht an fehlende Hardware gebunden, gehören aber nicht zum ersten Refactoring.

- **RTC-Modul (z. B. DS3231)** am vorhandenen I2C-Bus. Damit stimmt die Uhrzeit auch nach einem Neustart ohne WLAN,
  und die festen Spülzeiten laufen komplett offline.
- **OTA-Update über WLAN.** Die Standard-Partitionstabelle hat bereits zwei App-Slots à 1,25 MB, und die Firmware
  belegt aktuell ca. 1 MB. Damit muss man zum Flashen nicht mehr an die Anlage.
- **Passwort für das Webpanel** (HTTP Basic Auth).
- **Spüldauern in HA einstellbar** (zunächst nur über das Webpanel).
- **Statistik:** Wassermenge pro Tag oder Woche.
