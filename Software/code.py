import board
import busio
import adafruit_tlv493d
import digitalio
import math
import time
import sys
import supervisor
import json
import storage

# I2C Setup für den TLV493D-A1B6
i2c = busio.I2C(board.GP17, board.GP16)  # SCL = GP17, SDA = GP16
sensor = adafruit_tlv493d.TLV493D(i2c)
sensor.fast_mode = True  # Fast Mode aktivieren für schnellere Abtastrate

# GPIO20 als Ausgang konfigurieren
pin = digitalio.DigitalInOut(board.GP20)
pin.direction = digitalio.Direction.OUTPUT

# Variablen zur Steuerung des Ablaufs, Kalibrierung und Tastenaktionen
running = False
calibrated_zero_angle = None  # Speichert den Nullpunktwinkel
waiting_for_step_target = False  # Kontrolliert das High-Signal bis zur Zielstufe
abort_step_target = False  # Zum Abbrechen des Wartens auf das Ziel
step_target = 28  # Standardmäßig Stufe 28 als Zielstufe

# Funktion zum Laden der Konfiguration
def load_config():
    global calibrated_zero_angle, step_target
    try:
        with open("config.json", "r") as f:
            config = json.load(f)
            calibrated_zero_angle = config.get("calibrated_zero_angle", 0)
            step_target = config.get("step_target", 28)  # Lade den Ziel-STEP
            print(f"Konfiguration geladen: Nullpunktwinkel = {calibrated_zero_angle:.2f}°, Zielstufe = {step_target}.")
    except Exception as e:
        print(f"Fehler beim Laden der Konfiguration: {e}. Verwende Nullpunktwinkel = 0° und Zielstufe = 28.")

# Funktion zum Speichern der Konfiguration
def save_config():
    global calibrated_zero_angle
    config = {
        "step_target": step_target,
        "calibrated_zero_angle": calibrated_zero_angle
    }
    # Deaktiviere den Schreibschutz, bevor du schreibst
    storage.remount("/", readonly=False)
    try:
        with open("config.json", "w") as f:
            json.dump(config, f)
        print("Konfiguration gespeichert!")
    except Exception as e:
        print(f"Fehler beim Speichern der Konfiguration: {e}")
    finally:
        # Setze den Schreibschutz nach dem Schreiben wieder
        storage.remount("/", readonly=True)

# Funktion zum Berechnen des Rotationswinkels in Grad
def calculate_rotation(x, y):
    angle_radians = math.atan2(y, x)
    angle_degrees = math.degrees(angle_radians)
    return angle_degrees + 360 if angle_degrees < 0 else angle_degrees

# Funktion zum Berechnen der Stufe auf Basis des Winkels und des Nullpunkts
def calculate_step(angle, zero_angle):
    adjusted_angle = (zero_angle - angle) % 360  # Richtung ändern (Uhrzeigersinn)
    step = int(adjusted_angle / (360 / 62))
    return step

# Funktion zur Mittelung der Sensorwerte
def average_magnetic_values(n_samples=5):
    x_total, y_total, z_total = 0, 0, 0
    for _ in range(n_samples):
        magnetic = sensor.magnetic
        x_total += magnetic[0]
        y_total += magnetic[1]
        z_total += magnetic[2]
        time.sleep(0.01)  # Kurze Pause zwischen den Messungen
    return (x_total / n_samples, y_total / n_samples, z_total / n_samples)

# Funktion zum Starten, Stoppen, Kalibrieren und Triggern der Stufe
def check_serial_input():
    global running, calibrated_zero_angle, waiting_for_step_target, abort_step_target
    if supervisor.runtime.serial_bytes_available:
        command = sys.stdin.read(1)  # Liest nur ein Byte aus dem Puffer
        if command == 's':
            running = not running  # Ablauf starten/stoppen
            if running:
                print("Ablauf gestartet!")
            else:
                print("Ablauf gestoppt!")
        elif command == "0":
            # Nullpunkt kalibrieren (einmalige Messung)
            magnetic = average_magnetic_values()  # Durchschnittswerte abfragen
            calibrated_zero_angle = calculate_rotation(magnetic[0], magnetic[1])
            print(f"Nullpunkt kalibriert bei {calibrated_zero_angle:.2f}° (Stufe 0)")
            save_config()  # Speichere den Nullpunkt
        elif command == 'x':
            if waiting_for_step_target:
                # Abbrechen des Wartens auf die Zielstufe
                waiting_for_step_target = False
                abort_step_target = True
                pin.value = False  # Setze den GPIO20 auf LOW
                print("Warten auf Zielstufe abgebrochen, GPIO20 auf LOW.")
            else:
                # Setzt den Ausgang auf HIGH und wartet auf die Zielstufe
                if calibrated_zero_angle is not None:
                    waiting_for_step_target = True
                    abort_step_target = False
                    pin.value = True  # Setzt den GPIO20 auf HIGH
                    print(f"GPIO20 auf HIGH gesetzt. Warte auf Zielstufe {step_target}.")

# "Task" zur schnellen Abfrage der Stufe
def fast_step_check():
    global waiting_for_step_target, abort_step_target
    if calibrated_zero_angle is not None and waiting_for_step_target and not abort_step_target:
        # Abfrage der magnetischen Daten vom Sensor
        magnetic = average_magnetic_values()  # Durchschnittswerte abfragen
        angle = calculate_rotation(magnetic[0], magnetic[1])

        # Stufe berechnen
        step = calculate_step(angle, calibrated_zero_angle)

        # Aktuelle Stufe über die serielle Schnittstelle ausgeben
        print(f"Aktuelle Stufe: {step}")

        # Wenn die Zielstufe erreicht ist
        if step == step_target:
            pin.value = False  # Setzt den GPIO20 auf LOW
            waiting_for_step_target = False  # Beendet das Warten auf die Zielstufe
            print(f"Zielstufe {step_target} erreicht. GPIO20 auf LOW gesetzt.")
            time.sleep(0.05)  # Warten für 50ms

# Hauptprogramm
load_config()  # Konfiguration beim Start laden
last_output_time = time.monotonic()  # Variable zur Steuerung der Taktung
while True:
    check_serial_input()  # Überprüfen der Eingaben
    fast_step_check()  # Schnelle Abfrage der Stufe

    if running and calibrated_zero_angle is not None:
        current_time = time.monotonic()
        if current_time - last_output_time >= 1.0:
            last_output_time = current_time
            
            # Sofortige Abfrage der Magnetfeld-Daten vom Sensor
            magnetic = average_magnetic_values()  # Durchschnittswerte abfragen

            # Rotationswinkel berechnen
            angle = calculate_rotation(magnetic[0], magnetic[1])

            # Stufe berechnen
            step = calculate_step(angle, calibrated_zero_angle)

            # Winkel und Stufe auf der Konsole ausgeben
            print(f"Winkel: {angle:.2f}°, Stufe: {step}")

            # GPIO20 für 50ms auf HIGH setzen
            if not waiting_for_step_target:
                pin.value = True
                time.sleep(0.05)  # 50 ms warten
                pin.value = False
