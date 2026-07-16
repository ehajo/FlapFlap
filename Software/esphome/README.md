# FlapFlap

ESPHome-Projekt für eine historische Fallblattanzeige mit Magnetfeldsensor-Rückmeldung und pulseweiser Motoransteuerung.

## Ziel des Projekts

Dieses Projekt steuert eine Fallblattanzeige mit einem ESP32-C3 Super Mini.  
Die Blattposition wird über einen **TLV493D 3D-Magnetfeldsensor** erfasst.  
Der Motor wird über einen **MOC3043** angesteuert.  
Die Anzeige bewegt sich **nur vorwärts**.  
Ziel ist eine robuste Anfahrt eines gewünschten Blatts mit Rückmeldung über den Sensor.

## Hardware

### Controller
- ESP32-C3 Super Mini

### Sensor
- Infineon TLV493D-A1B6
- I2C-Adresse: `0x5E`

### Aktorik
- MOC3043 zur Ansteuerung des Motors

### Bedienelement
- Taster gegen GND, interner Pull-up im ESP32-C3

## Verbindliche Pinbelegung

- **TLV493D SDA** → `GPIO0`
- **TLV493D SCL** → `GPIO1`
- **Taster** → `GPIO3` gegen GND
- **MOC3043 Eingang** → `GPIO10`

## Wichtige Hardware-Hinweise

### Pull-ups
Für I2C sind externe Pull-ups erforderlich:
- SDA → 3V3 über **4,7 kΩ**
- SCL → 3V3 über **4,7 kΩ**

Keine 1-kΩ-Pull-ups verwenden.

### Versorgung
- TLV493D nur mit **3,3 V**
- keine 5-V-Pegel an ESP32-C3-GPIOs

### ESP32-C3 Pin-Einschränkungen
Diese Pins möglichst nicht für das Projekt verwenden:
- `GPIO2`, `GPIO8`, `GPIO9` → Strapping-/Bootstrap-Pins
- `GPIO4` bis `GPIO7` → JTAG-belegt bzw. vermeiden
- `GPIO18`, `GPIO19` → USB Serial/JTAG
- `GPIO20`, `GPIO21` → nach Möglichkeit frei lassen

## Software-Stack

- **ESPHome 2026.3.2**
- Arduino-Framework auf ESP32-C3
- lokaler External Component für den TLV493D und die Regelung

## Repo-Struktur

```text
.
├── flapflap.yaml
├── my_components/
│   └── tlv493d_test/
│       ├── __init__.py
│       ├── tlv493d_test.h
│       └── tlv493d_test.cpp
└── README.md
```

## Aktueller Funktionsumfang

Der aktuelle Stand kann:

- TLV493D per I2C ansprechen
- Magnetfeldwerte X/Y/Z lesen
- Winkel aus X/Y berechnen
- Winkel glätten und stabilisieren
- aktuelle Blattnummer aus Winkel und Nullwinkel bestimmen
- auf ein beliebiges Blatt kalibrieren
- Motor pulseweise ansteuern
- ein Zielblatt anfahren
- Timeout und fehlenden Fortschritt erkennen

## Grundannahmen der Regelung

- Die Anzeige bewegt sich nur **vorwärts**
- Die typische Drehgeschwindigkeit liegt ungefähr bei **5 Blättern pro Sekunde**
- Die Blattnummer wird aus dem Sensorwinkel mit Hysterese bestimmt
- Die Sensorlogik muss im Stillstand stabil sein, darf aber während der Bewegung nicht zu träge werden

## Wichtige ESPHome-Parameter

Typische relevante Parameter im Component:
- `pages`
- `zero_angle`
- `calibration_leaf`
- `target_leaf`
- `sensor_samples`
- `motor_pulse_ms`
- `motor_settle_ms`
- `move_timeout_ms`
- `max_pulses_per_move`
- `unchanged_limit`

## Build

Projekt kompilieren:

```bash
esphome compile flapflap.yaml
```

Projekt flashen / ausführen:

```bash
esphome run flapflap.yaml
```

Logs ansehen:

```bash
esphome logs flapflap.yaml
```

## Typischer Testablauf

1. Hardware prüfen
2. I2C-Scan prüfen, Sensor muss auf `0x5E` erscheinen
3. Sensorwerte X/Y/Z und Winkel prüfen
4. Kalibrierblatt setzen
5. `Hier kalibrieren` auslösen
6. prüfen, ob `Aktuelles Blatt` stabil ist
7. `Zielblatt` setzen
8. `Zu Zielblatt fahren` testen
9. bei Problemen Logs auf Blattwechsel, Pulse und Status prüfen

## Bekannte Besonderheiten

- Der TLV493D kann nach problematischen I2C-Transaktionen in einem ungünstigen Zustand hängen. In solchen Fällen hilft oft ein vollständiger Power-Cycle.
- Blattgrenzen sind empfindlich; deshalb sind Mittelung, Vektorfilterung und Hysterese wichtig.
- Übergänge um 0°/360° müssen besonders robust behandelt werden.

## Erwartungen an Änderungen

Bitte Änderungen **minimal und gezielt** halten.

### Nicht ohne Not ändern
- Pinbelegung
- I2C-Adresse
- Kalibrierlogik-Grundprinzip
- Blattberechnung nur vorwärts
- ESPHome-Version-Kompatibilität zu 2026.3.2

### Bevorzugte Arbeitsweise
- kleine, nachvollziehbare Änderungen
- bestehende Logik erhalten
- zuerst Buildfehler beheben
- danach Kompilierung erneut prüfen
- keine unnötigen Refactorings

## Definition of Done

Eine Änderung ist fertig, wenn:

1. `esphome compile flapflap.yaml` erfolgreich läuft
2. keine bestehenden Funktionen unbeabsichtigt entfernt wurden
3. Sensorwerte weiterhin plausibel sind
4. Kalibrierung weiterhin funktioniert
5. die Blattberechnung stabil bleibt
6. Motorsteuerung nicht dauerhaft hängen bleibt
7. Logs weiterhin hilfreich und präzise sind

## Hinweise für Coding Agents / Codex

Wenn du als Agent in diesem Repo arbeitest:

- lies zuerst diese Datei vollständig
- ändere nur die minimal nötigen Dateien
- erkläre kurz die Ursache eines Problems
- führe nach Änderungen `esphome compile flapflap.yaml` aus
- behebe verbleibende Buildfehler
- erhalte bestehende Hardware-Pins und Projektziele
- vermeide große Umbauten ohne ausdrückliche Aufforderung
- bevorzuge robuste, konservative Änderungen gegenüber kreativen Refactorings
