# AGENTS.md

## Projektkontext

Dieses Repository enthält ein ESPHome-Projekt für eine historische Fallblattanzeige.

Hardware:
- ESP32-C3 Super Mini
- TLV493D-A1B6 an I2C
- Taster gegen GND mit internem Pull-up
- MOC3043 zur Motoransteuerung

Ziel:
- Blattposition über Magnetfeldsensor erfassen
- Winkel stabil bestimmen
- Blattnummer robust berechnen
- Motor pulseweise vorwärts auf ein Zielblatt fahren

## Verbindliche Pinbelegung

- TLV493D SDA → `GPIO0`
- TLV493D SCL → `GPIO1`
- Taster → `GPIO3` gegen GND
- MOC3043 → `GPIO10`

Diese Pinbelegung nicht ohne ausdrückliche Anweisung ändern.

## Wichtige technische Randbedingungen

- ESPHome-Version: **2026.3.2**
- ESP32-C3 mit Arduino-Framework
- TLV493D I2C-Adresse: `0x5E`
- Anzeige fährt **nur vorwärts**
- Zielgeschwindigkeit: ungefähr **5 Blätter pro Sekunde**
- Pull-ups an SDA und SCL: **4,7 kΩ nach 3,3 V**

## Zu vermeidende Änderungen

Nicht ohne ausdrückliche Anweisung ändern:
- Pinbelegung
- I2C-Adresse
- Grundprinzip der Kalibrierung
- Vorwärtslogik der Blattbewegung
- ESPHome-Version-Kompatibilität
- funktionierende Sensorinitialisierung

## Erwartete Arbeitsweise

Bei Änderungen im Repo:
1. Ursache des Problems kurz analysieren
2. nur die minimal nötigen Dateien ändern
3. keine unnötigen Refactorings durchführen
4. bestehende Funktionen erhalten
5. nach Änderungen immer kompilieren

## Pflichtkommando nach Änderungen

Immer ausführen:

```bash
esphome compile flapflap.yaml
```

Wenn der Build fehlschlägt:
- Fehler analysieren
- gezielt beheben
- erneut kompilieren

## Definition of Done

Eine Aufgabe ist erst fertig, wenn:
- `esphome compile flapflap.yaml` erfolgreich ist
- keine bestehende Funktion unbeabsichtigt entfernt wurde
- Sensorwerte weiterhin plausibel sind
- Kalibrierung weiterhin funktioniert
- Blattberechnung stabil bleibt
- Motorsteuerung nicht hängen bleibt
- Logs weiterhin sinnvoll sind

## Repo-Struktur

Wichtige Dateien:
- `flapflap.yaml`
- `my_components/tlv493d_test/__init__.py`
- `my_components/tlv493d_test/tlv493d_test.h`
- `my_components/tlv493d_test/tlv493d_test.cpp`

## Hinweise für Codex

Arbeite konservativ.
Bevorzuge kleine, prüfbare Änderungen.
Wenn du etwas an der Regelung änderst, erkläre kurz warum.
Wenn du etwas an Sensorfilterung, Hysterese oder Motorsteuerung änderst, achte besonders darauf, dass die Anzeige im Stillstand stabil bleibt und während der Bewegung nicht zu träge wird.
