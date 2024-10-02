import board
import busio
import adafruit_tlv493d
import digitalio
import math
import time
import sys
import supervisor

# I2C setup für den TLV493D-A1B6
i2c = busio.I2C(board.GP17, board.GP16)  # SCL = GP17, SDA = GP16
sensor = adafruit_tlv493d.TLV493D(i2c)
sensor.fast_mode = True  # Setze den Sensor in den Fast Mode

# GPIO20 als Ausgang konfigurieren
pin = digitalio.DigitalInOut(board.GP20)
pin.direction = digitalio.Direction.OUTPUT

# Variablen zur Steuerung des Ablaufs, Kalibrierung und Tastenaktionen
running = False
calibrated_zero_angle = None  # Speichert den Nullpunktwinkel
waiting_for_step_28 = False  # Kontrolliert das High-Signal bis Stufe 28

# Funktion zur Berechnung des Rotationswinkels in Grad
def calculate_rotation(x, y):
    angle_radians = math.atan2(y, x)
    angle_degrees = math.degrees(angle_radians)
    return angle_degrees + 360 if angle_degrees < 0 else angle_degrees

# Funktion zur Berechnung der Stufe auf Basis des Winkels und des Nullpunkts
def calculate_step(angle, zero_angle):
    # Invertiere die Berechnung des relativen Winkels, um die Richtung umzukehren
    adjusted_angle = (zero_angle - angle) % 360  # Richtung ändern (Uhrzeigersinn)
    
    # Stufenberechnung (360 Grad in 62 Stufen)
    step = int(adjusted_angle / (360 / 62))
    
    return step

# Funktion zur Mittelung der Magnetfeldwerte
def average_magnetic_field(sensor, num_samples=10):
    sum_x, sum_y = 0, 0
    for _ in range(num_samples):
        magnetic = sensor.magnetic
        sum_x += magnetic[0]
        sum_y += magnetic[1]
        time.sleep(0.01)  # Kleine Verzögerung zwischen den Abtastungen
    
    avg_x = sum_x / num_samples
    avg_y = sum_y / num_samples
    return avg_x, avg_y

# Funktion zum Starten, Stoppen, Kalibrieren und Triggern der Stufe 28
def check_serial_input():
    global running, calibrated_zero_angle, waiting_for_step_28
    # Prüfen, ob Eingaben im serial buffer vorhanden sind
    if supervisor.runtime.serial_bytes_available:
        command = sys.stdin.read(1).strip()  # Liest ein Zeichen aus dem Puffer
        if command == 's':
            running = not running  # Ablauf starten/stoppen
            if running:
                print("Ablauf gestartet!")
            else:
                print("Ablauf gestoppt!")
        elif command == '0':
            # Nullpunkt kalibrieren (mittelte Messwerte)
            avg_x, avg_y = average_magnetic_field(sensor)
            calibrated_zero_angle = calculate_rotation(avg_x, avg_y)
            print(f"Nullpunkt kalibriert bei {calibrated_zero_angle:.2f}° (Stufe 0)")
        elif command == 'x':
            # Setzt den Ausgang auf HIGH und wartet auf Stufe 28
            if calibrated_zero_angle is not None:
                waiting_for_step_28 = True
                pin.value = True  # Setzt den GPIO20 auf HIGH
                print("GPIO20 auf HIGH gesetzt. Warte auf Stufe 28.")

# Hauptprogramm
while True:
    check_serial_input()  # Überprüft, ob die Taste 's', '0' oder 'x' gedrückt wurde
    
    if running and calibrated_zero_angle is not None:
        # Mitteln der Magnetfeld-Daten vom Sensor (10 Werte)
        avg_x, avg_y = average_magnetic_field(sensor)

        # Rotationswinkel berechnen
        angle = calculate_rotation(avg_x, avg_y)

        # Stufe berechnen (Nullpunkt = Stufe 0, Zählrichtung im Uhrzeigersinn)
        step = calculate_step(angle, calibrated_zero_angle)

        # Winkel und Stufe auf der Konsole ausgeben
        print(f"Winkel: {angle:.2f}°, Stufe: {step}")

        # Wenn der GPIO20 auf HIGH gesetzt wurde und die Stufe 28 erreicht ist oder alternativ der Widerstand zur Ansteuerung des Triacs abbrennt
        if waiting_for_step_28 and step == 28:
            pin.value = False  # Setzt den GPIO20 auf LOW
            waiting_for_step_28 = False  # Beendet das Warten auf Stufe 28
            print("Stufe 28 erreicht. GPIO20 auf LOW gesetzt.")

        # GPIO20 für 50ms auf HIGH setzen (normales Verhalten, wenn nicht auf Stufe 28 gewartet wird)
        if not waiting_for_step_28:
            pin.value = True
            time.sleep(0.05)  # 50 ms warten

            # GPIO20 auf LOW setzen
            pin.value = False

            # Warten, um den Rest der 1 Sekunde zu erreichen
            time.sleep(0.95)  # 950 ms warten (1 Sekunde insgesamt)

    time.sleep(0.1)  # Kurze Pause, um die CPU zu entlasten
