
# FlapFlap Sub-Clock & Uhr Software

Willkommen zu der FlapFlap-Software! Dieses Projekt umfasst zwei Hauptfunktionen: eine **normale Uhr** und eine **Sub-Clock**. Die Software läuft auf einem **Raspberry Pi Pico W** und verwendet den **TLV493D Magnetfeldsensor**, um die Zeit in verschiedenen Formaten zu messen und darzustellen. Sie kann für die Anzeige von Sekunden, Minuten und Stunden konfiguriert werden. Diese README erklärt die Funktionsweise, wie du die Software einrichtest und wie du die Konfiguration anpassen kannst.

## Funktionen

Die FlapFlap Software bietet eine Vielzahl von Funktionen, die auf den folgenden beiden Hauptvarianten basieren:

### 1. Normale Uhr

Die normale Uhr liest den Magnetfeldsensor aus, um den Rotationswinkel der Uhr zu berechnen. Der Mikrocontroller zeigt basierend auf diesem Winkel und der konfigurierten Logik die Zeit in **Sekunden**, **Minuten** oder **Stunden** an.

- **Sekunden**: Zeigt die Sekunden an, die über UART empfangen werden.
- **Minuten**: Zeigt die Minuten an, die über UART empfangen werden.
- **Stunden**: Zeigt die Stunden an, die über UART empfangen werden.

### 2. Sub-Clock

Die Sub-Clock ist eine alternative Anzeige, die speziell für eine reduzierte Zeitdarstellung gedacht ist. Sie empfängt dieselben Werte über den UART-Datenstrom, aber die Interpretation der Daten erfolgt je nach den spezifischen Anforderungen des Projekts (z. B. eine präzisere, abgestufte Anzeige).

## Systemanforderungen

- **Hardware**:
  - Raspberry Pi Pico W oder vergleichbare Mikrocontroller mit CircuitPython-Unterstützung.
  - TLV493D Magnetfeldsensor (I2C-Schnittstelle).
  - Eine UART-Schnittstelle für die serielle Kommunikation.
- **Software**:
  - CircuitPython für die Ausführung der Software.
  - `adafruit_tlv493d`-Bibliothek für den Magnetfeldsensor.

## Installation

### Abhängigkeiten

Für die Installation sind folgende Bibliotheken erforderlich:

- **Adafruit TLV493D**: Diese Bibliothek ist notwendig, um den Magnetfeldsensor über die I2C-Schnittstelle anzusprechen.
  
Installiere die Bibliothek über den CircuitPython-Installer oder lade sie manuell herunter.

### Code Setup

1. **Code auf den Raspberry Pi Pico W hochladen**:
   Lade die Python-Dateien auf den Raspberry Pi Pico W hoch und speichere sie im Hauptverzeichnis.
   
2. **I2C und UART konfigurieren**:
   Stelle sicher, dass die I2C- und UART-Verbindungen für deinen Raspberry Pi Pico W richtig eingerichtet sind.

3. **Konfigurationsdatei (`config.json`) erstellen**:
   Stelle sicher, dass eine gültige `config.json`-Datei im Hauptverzeichnis des Dateisystems des Raspberry Pi Pico W vorhanden ist. Diese Datei enthält alle Einstellungen für die Uhr und Sub-Clock.

## config.json

Die `config.json`-Datei speichert die Konfiguration für die Uhr, einschließlich des kalibrierten Nullpunkts und des Typs der angezeigten Zeit (Sekunden, Minuten, Stunden oder eine benutzerdefinierte Sub-Clock-Logik).

### Beispiel `config.json`:

```json
{
  "calibrated_zero_angle": 91.4311,
  "type": "Sekunden"
}
```

### Konfigurationsfelder:

- **`calibrated_zero_angle`**:
  - Dies ist der kalibrierte Nullpunktwinkel in Grad. Er gibt den Referenzwinkel an, ab dem die Uhr arbeitet. Dieser Wert wird durch die Kalibrierung des Magnetfeldsensors bestimmt.
  - **Wertbereich**: Ein numerischer Wert zwischen 0 und 360.
  - **Beispiel**: `91.4311`

- **`type`**:
  - Dieser Parameter bestimmt, welche Zeit angezeigt wird und wie der empfangene Datenstrom über UART interpretiert wird. Es gibt mehrere Optionen, die du je nach deinen Anforderungen wählen kannst.
  - **Mögliche Werte für `type`**:
    - `"Sekunden"`: Die Uhr zeigt die Sekunden an. Der empfangene Datenstrom steuert die Sekunden.
    - `"Minuten"`: Die Uhr zeigt die Minuten an. Der empfangene Datenstrom steuert die Minuten.
    - `"Stunden"`: Die Uhr zeigt die Stunden an. Der empfangene Datenstrom steuert die Stunden.
    - `"nix62"`: Diese Einstellung zeigt immer die Stufe 31 an, was in vielen Fällen für eine nicht funktionale Zeitdarstellung verwendet wird (beispielsweise für eine Sub-Clock, die keine genaue Zeit anzeigt).
    - `"nix40"`: Diese Einstellung zeigt immer die Stufe 26 an, eine weitere alternative Darstellung ohne genaue Zeit (kann für experimentelle Sub-Clock-Implementierungen verwendet werden).
    - **Beispiel für Konfigurationen**:
      - **Sekundenanzeige**:
        ```json
        {
          "calibrated_zero_angle": 91.4311,
          "type": "Sekunden"
        }
        ```
      - **Minutenanzeige**:
        ```json
        {
          "calibrated_zero_angle": 120.0,
          "type": "Minuten"
        }
        ```
      - **Stundenanzeige**:
        ```json
        {
          "calibrated_zero_angle": 180.5,
          "type": "Stunden"
        }
        ```
      - **Sub-Clock-Variante (nix62)**:
        ```json
        {
          "calibrated_zero_angle": 91.4311,
          "type": "nix62"
        }
        ```
      - **Sub-Clock-Variante (nix40)**:
        ```json
        {
          "calibrated_zero_angle": 91.4311,
          "type": "nix40"
        }
        ```

## Kalibrierung

Die Kalibrierung des Magnetfeldsensors erfolgt durch den Benutzer, um den Nullpunktwinkel festzulegen. Wenn du den Button (GPIO22) drückst, wird der Sensor neu kalibriert, und der aktuelle Winkel wird als Nullpunkt gespeichert. Der Nullpunkt wird dann in der `config.json`-Datei gespeichert, sodass er bei jedem Neustart des Geräts wieder verwendet werden kann.

### Kalibrierungsschritte:

1. Drücke den Kalibrierungs-Button (GPIO22).
2. Die Software misst das Magnetfeld und berechnet den Nullpunktwinkel.
3. Der neue Nullpunkt wird in der `config.json` gespeichert.

## UART-Datenstrom

Die Software empfängt über die serielle Schnittstelle UART bis zu drei Bytes:

1. **Sekunden (Typ `"Sekunden"`)**: Das erste Byte gibt die Zielstufe für die Sekunden an (Wertbereich: 0-62).
2. **Minuten (Typ `"Minuten"`)**: Das zweite Byte gibt die Zielstufe für die Minuten an (Wertbereich: 0-62).
3. **Stunden (Typ `"Stunden"`)**: Das dritte Byte gibt die Zielstufe für die Stunden an (Wertbereich: 0-40).

Die empfangenen Bytes werden verwendet, um die `step_target`-Variable zu setzen. Wenn die aktuelle Stufe der Zielstufe entspricht, wird die Anzeige entsprechend aktualisiert.

### Beispiel für einen gültigen Datenstrom:
- Wenn der `type` auf `"Sekunden"` gesetzt ist, wird das erste Byte die Sekunden definieren.
- Bei `"Minuten"` und `"Stunden"` werden die entsprechenden Bytes verwendet.

## Häufige Fehlerbehebung

- **Konfigurationsdatei fehlt**: Wenn die `config.json`-Datei nicht gefunden wird, wird ein Standardwert verwendet. Stelle sicher, dass die Datei korrekt erstellt und im richtigen Verzeichnis abgelegt wurde.
- **Datenübertragung funktioniert nicht**: Überprüfe die UART-Verbindung und stelle sicher, dass der Mikrocontroller korrekt mit dem Sender kommuniziert.
- **Magnetfeldsensor kalibrieren**: Falls die Kalibrierung fehlschlägt oder der Nullpunkt fehlerhaft ist, stelle sicher, dass der Magnetfeldsensor richtig angeschlossen und korrekt kalibriert wurde.

## Lizenz

Dieses Projekt ist unter der [MIT-Lizenz](LICENSE) lizenziert.

---

Für weitere Informationen oder Fragen besuche unsere Website: [eHaJo.de](https://www.eHaJo.de) oder schließe dich unserem [Twitch-Livestream](https://www.twitch.tv/eHaJo) an!
