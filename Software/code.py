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
import wifi
import socketpool
import adafruit_ntp
import rtc
import struct

# I2C Setup für den TLV493D-A1B6 an GP16 (SDA) und GP17 (SCL)
i2c = busio.I2C(board.GP17, board.GP16)
sensor = adafruit_tlv493d.TLV493D(i2c)
sensor.fast_mode = False  # Fast Mode deaktivieren

# DS3231-Uhr initialisieren
ds3231 = None
try:
    import adafruit_ds3231
    ds3231 = adafruit_ds3231.DS3231(i2c)
    print("DS3231 RTC initialisiert")
except ValueError:
    print("Kein DS3231 RTC verbunden")
    
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

# Funktion zur Ermittlung, ob Sommerzeit (MESZ) aktiv ist
def is_dst_europe(now):
    year = now.tm_year
    start_dst = time.mktime((year, 3, 31, 2, 0, 0, 6, 0, -1))
    while time.localtime(start_dst).tm_wday != 6:
        start_dst -= 86400
    end_dst = time.mktime((year, 10, 31, 3, 0, 0, 6, 0, -1))
    while time.localtime(end_dst).tm_wday != 6:
        end_dst -= 86400
    return start_dst <= time.mktime(now) < end_dst

# Funktion zur Umrechnung der UTC-Zeit auf die Berliner Zeitzone
def get_berlin_time(current_time):
    berlin_time = list(current_time)
    berlin_time[3] += 1
    if is_dst_europe(current_time):
        berlin_time[3] += 1
    berlin_time = time.localtime(time.mktime(tuple(berlin_time)))
    return berlin_time

# WLAN-Verbindung
def connect_to_wifi():
    try:
        with open("wlan.json", "r") as f:
            config = json.load(f)
            ssid = config.get("wifi_ssid")
            password = config.get("wifi_password")
            if ssid and password:
                print(f"Verbinde mit {ssid}...")
                wifi.radio.connect(ssid, password)
                print("WLAN verbunden!")
            else:
                print("WLAN-Zugangsdaten nicht gefunden.")
    except OSError:
        print("Fehler beim Laden der WLAN-Konfiguration.")

# Funktion zur Abfrage der aktuellen Zeit über NTP oder DS3231
def get_time():
    try:
        if ds3231:
            return ds3231.datetime
        elif wifi.radio.ipv4_address:
            pool = socketpool.SocketPool(wifi.radio)
            ntp = adafruit_ntp.NTP(pool)
            current_time = ntp.datetime
            berlin_time = get_berlin_time(current_time)
            rtc.RTC().datetime = berlin_time
            if ds3231:
                ds3231.datetime = berlin_time
            # print("Aktuelle Uhrzeit (Berlin): ", berlin_time)
            return berlin_time
    except Exception as e:
        print(f"Fehler bei Zeitabfrage: {e}")
    return rtc.RTC().datetime

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

# Hauptprogramm
print("FlapFlap Version 0.9.2")
print("(c) eHaJo, 2024, Twitch-Livestream-Projekt")
load_config()
connect_to_wifi()
get_time()
last_output_time = time.monotonic()

running = True

current_rtc_time = get_time()
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
        current_rtc_time = get_time() # Uhrzeit abfragen
        # Minuten und Sekunden in 2 Bytes verpacken:
        bytes_to_send = struct.pack(">H", current_rtc_time.tm_min) + struct.pack(">H", current_rtc_time.tm_sec)
        uart.write(bytes_to_send) # Daten losschicken
        if type == "Sekunden":
            step_target = current_rtc_time.tm_sec
            if step_target > 30:
                step_target += 1
        elif type == "Minuten":
            step_target = current_rtc_time.tm_min
            if step_target > 30:
                step_target += 1
        else: 
            step_target = current_rtc_time.tm_hour
        if step != step_target:
            print("Flap.")
            continue
        time.sleep(0.2)
    else:
        pin.value = True
        magnetic = average_magnetic_field(sensor)
        magnetic = average_magnetic_field(sensor)
        angle = calculate_rotation(magnetic[0], magnetic[1])
        step = calculate_step(angle, calibrated_zero_angle)
        print(f"Winkel: {angle:.2f}°, Aktuelle Stufe: {step}, Zielstufe: {step_target}")
        time.sleep(0.01)
