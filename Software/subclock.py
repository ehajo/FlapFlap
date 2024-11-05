import time
import board
import busio
import adafruit_tlv493d
import digitalio
import math
import json
import sys
import storage
import supervisor
import struct

# I2C Setup für den TLV493D-A1B6 an GP16 (SDA) und GP17 (SCL)
i2c = busio.I2C(board.GP17, board.GP16)
sensor = adafruit_tlv493d.TLV493D(i2c)
sensor.fast_mode = False  # Fast Mode deaktivieren
    
# UART-Initialisierung auf UART0 (TX=Pin 0, RX=Pin 1)
uart = busio.UART(board.GP0, board.GP1, baudrate=9600, timeout=0.1)

# GPIO21 als Ausgang konfigurieren
pin = digitalio.DigitalInOut(board.GP21)
pin.direction = digitalio.Direction.OUTPUT

# Taste an GPIO22 mit internem Pull-up-Widerstand
button = digitalio.DigitalInOut(board.GP22)
button.direction = digitalio.Direction.INPUT
button.pull = digitalio.Pull.UP

# Variablen
running = False
calibrated_zero_angle = 0  # Standard Nullpunktwinkel
step_target = 0  # Initiales Ziel-Stufe
abort_step_target = False

# Funktion zur Berechnung des Rotationswinkels
def calculate_rotation(x, y):
    angle_radians = math.atan2(y, x)
    angle_degrees = math.degrees(angle_radians)
    return angle_degrees + 360 if angle_degrees < 0 else angle_degrees

# Funktion zur Berechnung der Stufe auf Basis des Winkels und Nullpunkts
def calculate_step(angle, zero_angle):
    global pages
    if type == "Sekunden": 
        pages = 62
    elif type == "Minuten":
        pages = 62
    else: 
        pages = 40
    adjusted_angle = (zero_angle - angle) % 360
    return int(adjusted_angle / (360 / pages))

# Funktion zur Mittelung von Sensorwerten
def average_magnetic_field(sensor, num_samples=3):
    total_x, total_y = 0, 0
    for _ in range(num_samples):
        magnetic = sensor.magnetic
        total_x += magnetic[0]
        total_y += magnetic[1]
    return total_x / num_samples, total_y / num_samples

# Funktion zum Laden der Konfiguration (Nullpunktwinkel)
def load_config():
    global calibrated_zero_angle
    global type
    try:
        with open("/config.json", "r") as f:
            config = json.load(f)
            calibrated_zero_angle = config.get("calibrated_zero_angle", 1.234)
            print(f"Konfiguration geladen: Nullpunkt = {calibrated_zero_angle}")
            type = config.get("type", "Minuten")
            if type == "Sekunden":
                print("Ich bin eine Sekundenanzeige!")
            elif type == "Minuten":
                print("Ich bin eine Minutenanzeige!")
            else:
                print("Ich bin eine Stundenanzeige!")
    except OSError:
        print("Keine Konfigurationsdatei gefunden. Verwende Standardwerte.")

# Funktion zum Speichern der Konfiguration      
def save_config():
    global calibrated_zero_angle
    global type
    config = {
        "calibrated_zero_angle": calibrated_zero_angle,
        "type": type
    }
    storage.remount("/", readonly=False, disable_concurrent_write_protection=True)
    try:
        with open("config.json", "w") as f:
            json.dump(config, f)
        print("Konfiguration gespeichert!")
    except Exception as e:
        print(f"Fehler beim Speichern der Konfiguration: {e}")
    finally:
        storage.remount("/", readonly=True)

# Nullpunktkalibrierung
def calibrate_zero_point():
    global calibrated_zero_angle
    magnetic = average_magnetic_field(sensor, 1) 
    magnetic = average_magnetic_field(sensor, 10)
    calibrated_zero_angle = calculate_rotation(magnetic[0], magnetic[1])
    print(f"Nullpunkt kalibriert bei {calibrated_zero_angle:.2f}°")
    save_config()

# Funktion, die durch UART-Read aufgerufen wird
def uart_read():
    global step_target
    data = uart.read(32)  # Maximal 32 Bytes auf einmal lesen

    if data:
        # Wenn mindestens zwei Bytes empfangen wurden, die ersten beiden als Ganzzahl speichern
        if len(data) >= 2:
            step_target = int.from_bytes(data[:2], "big")  # Bytes als Ganzzahl interpretieren (big-endian)
            print(f"Daten empfangen: step_target: {step_target}, Anzahl Bytes: {len(data)}")
            uart.write(data[2:])  # Übrige Bytes zurückschicken
        else:
            uart.write(data)  # Falls weniger als zwei Bytes, alle Daten zurückschicken

# Hauptprogramm
print("FlapFlap SubClock Version 0.1.0")
print("(c) eHaJo, 2024, Twitch-Livestream-Projekt")
load_config()

running = True

if type == "Sekunden":
    step_target = current_rtc_time.tm_sec
elif type == "Minuten":
    step_target = current_rtc_time.tm_min
else: 
    step_target = current_rtc_time.tm_hour
    
magnetic = average_magnetic_field(sensor)
magnetic = average_magnetic_field(sensor)
angle = calculate_rotation(magnetic[0], magnetic[1])
step = calculate_step(angle, calibrated_zero_angle)
if type == "Minuten" or type == "Sekunden":
    if step_target > 30:
        step_target += 1

while True:
    if not button.value:
        calibrate_zero_point()
        time.sleep(0.5)
    if step == step_target:
        pin.value = False
        uart_read()
        if type == "Sekunden":
            if step_target > 30:
                step_target += 1
        elif type == "Minuten":
            if step_target > 30:
                step_target += 1
        if step != step_target:
            print("Flap.")
            continue
        time.sleep(0.1)
    else:
        pin.value = True
        magnetic = average_magnetic_field(sensor)
        magnetic = average_magnetic_field(sensor)
        angle = calculate_rotation(magnetic[0], magnetic[1])
        step = calculate_step(angle, calibrated_zero_angle)
        print(f"Winkel: {angle:.2f}°, Aktuelle Stufe: {step}, Zielstufe: {step_target}")
        time.sleep(0.01)
