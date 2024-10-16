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

# I2C Setup für den TLV493D-A1B6 an GP16 (SDA) und GP17 (SCL)
i2c = busio.I2C(board.GP17, board.GP16)
sensor = adafruit_tlv493d.TLV493D(i2c)
sensor.fast_mode = False  # Fast Mode aktivieren

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
    adjusted_angle = (zero_angle - angle) % 360
    return int(adjusted_angle / (360 / 62))

# Funktion zur Mittelung von Sensorwerten
def average_magnetic_field(sensor, num_samples=3):
    total_x, total_y = 0, 0
    for _ in range(num_samples):
        magnetic = sensor.magnetic
        total_x += magnetic[0]
        total_y += magnetic[1]
        # time.sleep(0.01)  # Zeit zwischen den Messungen
    return total_x / num_samples, total_y / num_samples

# Hier das Uhrzeit Zeugs:
# Funktion zur Ermittlung, ob Sommerzeit (MESZ) aktiv ist
def is_dst_europe(now):
    """Überprüft, ob Sommerzeit (MESZ) für Europa gilt"""
    year = now.tm_year
    # Sommerzeit beginnt am letzten Sonntag im März
    start_dst = time.mktime((year, 3, 31, 2, 0, 0, 6, 0, -1))
    while time.localtime(start_dst).tm_wday != 6:  # Wochentag prüfen (Sonntag = 6)
        start_dst -= 86400  # Einen Tag zurückgehen

    # Sommerzeit endet am letzten Sonntag im Oktober
    end_dst = time.mktime((year, 10, 31, 3, 0, 0, 6, 0, -1))
    while time.localtime(end_dst).tm_wday != 6:  # Wochentag prüfen (Sonntag = 6)
        end_dst -= 86400  # Einen Tag zurückgehen

    # Prüfen, ob wir uns in der Sommerzeit befinden
    return start_dst <= time.mktime(now) < end_dst

# Funktion zur Umrechnung der UTC-Zeit auf die Berliner Zeitzone
def get_berlin_time(current_time):
    # Konvertiere die UTC-Zeit in lokale Zeit (MEZ/MESZ)
    berlin_time = list(current_time)
    
    # MEZ ist UTC + 1 Stunde
    berlin_time[3] += 1
    
    # Prüfen, ob Sommerzeit (MESZ) aktiv ist
    if is_dst_europe(current_time):
        berlin_time[3] += 1  # Sommerzeit ist UTC + 2 Stunden

    # Um die Zeit korrekt zu behandeln (Überlauf bei Stunden)
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

# Funktion zur Abfrage der aktuellen Zeit über NTP und Umrechnung auf Berliner Zeit
def get_ntp_time():
    try:
        pool = socketpool.SocketPool(wifi.radio)
        ntp = adafruit_ntp.NTP(pool)
        current_time = ntp.datetime  # Hole UTC-Zeit
        berlin_time = get_berlin_time(current_time)  # Wandle in Berliner Zeit um
        rtc.RTC().datetime = berlin_time  # Systemzeit setzen
        print("Aktuelle Uhrzeit (Berlin): ", berlin_time)
        return berlin_time
    except Exception as e:
        print(f"Fehler beim Abrufen der NTP-Zeit: {e}")
        return None

# Funktion zum Laden der Konfiguration (Nullpunktwinkel)
def load_config():
    global calibrated_zero_angle
    try:
        with open("/config.json", "r") as f:
            config = json.load(f)
            calibrated_zero_angle = config.get("calibrated_zero_angle", 0)
            print(f"Konfiguration geladen: Nullpunkt = {calibrated_zero_angle}")
    except OSError:
        print("Keine Konfigurationsdatei gefunden. Verwende Standardwerte.")

# Funktion zum Speichern der Konfiguration      
def save_config():
    global calibrated_zero_angle
    config = {
        "calibrated_zero_angle": calibrated_zero_angle
    }
    # Deaktiviere den Schreibschutz, bevor du schreibst
    storage.remount("/", readonly=False, disable_concurrent_write_protection=True)
    try:
        with open("config.json", "w") as f:
            json.dump(config, f)
        print("Konfiguration gespeichert!")
    except Exception as e:
        print(f"Fehler beim Speichern der Konfiguration: {e}")
    finally:
        # Setze den Schreibschutz nach dem Schreiben wieder
        storage.remount("/", readonly=True)

# Nullpunktkalibrierung
def calibrate_zero_point():
    global calibrated_zero_angle
    magnetic = average_magnetic_field(sensor)
    calibrated_zero_angle = calculate_rotation(magnetic[0], magnetic[1])
    print(f"Nullpunkt kalibriert bei {calibrated_zero_angle:.2f}°")
    save_config()  # Speichere die Kalibrierung in der Konfigurationsdatei

# Hauptprogramm
print("FlapFlap Version 0.9.1")
print("(c) eHaJo, 2024, Twitch-Livestream-Projekt")
load_config()  # Lade die Konfigurationswerte beim Start
connect_to_wifi()
get_ntp_time()  # Zeit bei Programmstart abrufen
last_output_time = time.monotonic()  # Für die 1-Sekunden-Taktung

# Hauptschleife
running = True  # Starte direkt im laufenden Zustand

# Aktualisiere die Zielstufe auf die aktuellen Minuten
current_rtc_time = rtc.RTC().datetime
step_target = current_rtc_time.tm_min  # Zielstufe auf Minuten setzen

magnetic = average_magnetic_field(sensor)
angle = calculate_rotation(magnetic[0], magnetic[1])
step = calculate_step(angle, calibrated_zero_angle)
if step_target > 30: # Leerblatt bei 31, also +1
    step_target = step_target + 1

while True:
    # Überprüfung, ob Taste gedrückt ist (gegen Masse gezogen)
    # TODO: Calibration-Modus erstellen. Wenn Taste gedrückt, die Automatik unten ausschalten!
    if not button.value:
        calibrate_zero_point()
        time.sleep(0.5)  # Entprellen der Taste

    # Überprüfen, ob die Zielstufe erreicht wurde
    if step == step_target:
        pin.value = False
        # Aktualisiere die Zielstufe auf die aktuellen Minuten
        current_rtc_time = rtc.RTC().datetime
        step_target = current_rtc_time.tm_min  # Zielstufe auf aktuelle Minute setzen
        if step_target > 30: # Leerblatt bei 31, also +1
            step_target = step_target + 1
        print(f"Zielstufe {step_target} erreicht. Zeit: {current_rtc_time.tm_hour}:{current_rtc_time.tm_min}:{current_rtc_time.tm_sec}")
        if step != step_target:
            continue
        time.sleep(1)  # Im Sekundentakt, das sollte reichen!
    else:
        pin.value = True  # Setze GPIO21 auf HIGH, solange die Zielstufe nicht erreicht ist
        # Sensorwerte lesen und aktuelle Stufe berechnen und zwar nur, wenn Zielstufe sich geändert hat
        magnetic = average_magnetic_field(sensor)
        angle = calculate_rotation(magnetic[0], magnetic[1])
        step = calculate_step(angle, calibrated_zero_angle)
        print(f"Winkel: {angle:.2f}°, Aktuelle Stufe: {step}, Zielstufe: {step_target}")
        time.sleep(0.01)  # GPIO21 für 50ms auf HIGH setzen

