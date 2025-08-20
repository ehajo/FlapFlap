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
from adafruit_datetime import datetime
from adafruit_httpserver import Server, Request, Response

# Konstanten
PAGES_SEKUNDEN_MINUTEN = 62
PAGES_STUNDEN = 40
LEERBLATT_SEKUNDEN_MINUTEN = 31
LEERBLATT_STUNDEN = 26
DEFAULT_ZERO_ANGLE = 1.234
DEFAULT_TYPE = "Minuten"
DEFAULT_CALIBRATION_LEAF = 0
MOTOR_PULSE_DURATION = 0.1  # 0,1 Sekunden für Motorpuls

# Globale Webserver-Variable
server = None

# I2C Setup für den TLV493D-A1B6 an GP16 (SDA) und GP17 (SCL)
i2c = busio.I2C(board.GP17, board.GP16)
sensor = adafruit_tlv493d.TLV493D(i2c)
sensor.fast_mode = False  # Fast Mode deaktivieren

# UART-Initialisierung auf UART0 (TX=Pin 0, RX=Pin 1)
uart = busio.UART(board.GP0, board.GP1, baudrate=9600, timeout=0.01)

# GPIO21 als Ausgang konfigurieren
pin = digitalio.DigitalInOut(board.GP21)
pin.direction = digitalio.Direction.OUTPUT

# Taste an GPIO22 mit internem Pull-up-Widerstand
button = digitalio.DigitalInOut(board.GP22)
button.direction = digitalio.Direction.INPUT
button.pull = digitalio.Pull.UP

# Variablen
running = False
calibrated_zero_angle = DEFAULT_ZERO_ANGLE
type = DEFAULT_TYPE
calibration_leaf = DEFAULT_CALIBRATION_LEAF
step_target = 0
abort_step_target = False

# Logfunktion
def log_error(message):
    try:
        storage.remount("/", readonly=False, disable_concurrent_write_protection=True)
        with open("/error.log", "a") as f:
            f.write(f"{time.localtime()} - {message}\n")
    except Exception as e:
        print(f"Logfehler: {e}")
    finally:
        storage.remount("/", readonly=True)

# Funktion zur Berechnung des Rotationswinkels
def calculate_rotation(x, y):
    angle_radians = math.atan2(y, x)
    angle_degrees = math.degrees(angle_radians)
    return angle_degrees + 360 if angle_degrees < 0 else angle_degrees

# Funktion zur Berechnung der Stufe auf Basis des Winkels und Nullpunkts
def calculate_step(angle, zero_angle):
    pages = PAGES_SEKUNDEN_MINUTEN if type in ["Sekunden", "Minuten", "nix62", "Kalibrierung"] else PAGES_STUNDEN
    adjusted_angle = (zero_angle - angle) % 360
    return int(adjusted_angle / (360 / pages))

# Funktion zur Mittelung von Sensorwerten
def average_magnetic_field(sensor, num_samples=3):
    total_x, total_y = 0, 0
    for _ in range(num_samples):
        magnetic = sensor.magnetic
        if magnetic[0] == 0 and magnetic[1] == 0:
            raise ValueError("Ungültige Sensordaten")
        total_x += magnetic[0]
        total_y += magnetic[1]
    return total_x / num_samples, total_y / num_samples

# Funktion zum Laden der Konfiguration
def load_config():
    global calibrated_zero_angle, type, calibration_leaf
    try:
        with open("/config.json", "r") as f:
            config = json.load(f)
            calibrated_zero_angle = config.get("calibrated_zero_angle", DEFAULT_ZERO_ANGLE)
            type = config.get("type", DEFAULT_TYPE)
            calibration_leaf = config.get("calibration_leaf", DEFAULT_CALIBRATION_LEAF)
            print(f"Konfiguration geladen: Nullpunkt = {calibrated_zero_angle}, Typ = {type}, Kalibrierungsblatt = {calibration_leaf}")
            if type == "Sekunden":
                print("Ich bin eine Sekundenanzeige!")
            elif type == "Minuten":
                print("Ich bin eine Minutenanzeige!")
            elif type == "Kalibrierung":
                print("Ich bin im Kalibrierungsmodus!")
            else:
                print("Ich bin eine Stundenanzeige!")
    except OSError as e:
        print(f"Keine Konfigurationsdatei gefunden: {e}")
        log_error(f"Konfigurationsfehler: {e}")

# Funktion zum Speichern der Konfiguration
def save_config():
    config = {
        "calibrated_zero_angle": calibrated_zero_angle,
        "type": type,
        "calibration_leaf": calibration_leaf
    }
    storage.remount("/", readonly=False, disable_concurrent_write_protection=True)
    try:
        with open("config.json", "w") as f:
            json.dump(config, f)
        print("Konfiguration gespeichert!")
    except Exception as e:
        print(f"Fehler beim Speichern der Konfiguration: {e}")
        log_error(f"Konfigurationsspeicherfehler: {e}")
    finally:
        storage.remount("/", readonly=True)

# Nullpunktkalibrierung
def calibrate_zero_point():
    global calibrated_zero_angle
    try:
        magnetic = average_magnetic_field(sensor, num_samples=1)
        magnetic = average_magnetic_field(sensor, num_samples=10)
        calibrated_zero_angle = calculate_rotation(magnetic[0], magnetic[1])
        print(f"Nullpunkt kalibriert bei {calibrated_zero_angle:.2f}°")
        save_config()
    except ValueError as e:
        print(f"Kalibrierungsfehler: {e}")
        log_error(f"Kalibrierungsfehler: {e}")

# Funktion zum Vorwärtsbewegen des Motors um ein Blatt
def advance_leaf():
    try:
        pin.value = True
        time.sleep(MOTOR_PULSE_DURATION)
        pin.value = False
        print("Motor für 0,1s aktiviert (Blatt weiter)")
        log_error("Motor für 0,1s aktiviert")
    except Exception as e:
        print(f"Fehler beim Vorwärtsbewegen: {e}")
        log_error(f"Fehler beim Vorwärtsbewegen: {e}")

# Funktion, die durch UART-Read aufgerufen wird
def uart_read():
    global step_target
    data = uart.read(32)  # Maximal 32 Bytes auf einmal lesen
    if data and type != "Kalibrierung":  # Ignoriere UART im Kalibrierungsmodus
        if len(data) == 3:
            if type == "Sekunden":
                step_target = data[0]
                if step_target > 62:
                    step_target = 0
                if step_target > 30:
                    step_target += 1
            elif type == "Minuten":
                step_target = data[1]
                if step_target > 62:
                    step_target = 0
                if step_target > 30:
                    step_target += 1
            elif type == "Stunden":
                step_target = data[2]
                if step_target > 40:
                    step_target = 0
            elif type == "nix62":
                step_target = LEERBLATT_SEKUNDEN_MINUTEN
            elif type == "nix40":
                step_target = LEERBLATT_STUNDEN
            else:
                step_target = 0
            print(f"Daten empfangen: step_target: {step_target}, Anzahl Bytes: {len(data)}")

# Funktion zum Leeren des UART-Eingangspuffers
def flush_uart_input():
    while uart.in_waiting > 0:
        uart.read(uart.in_waiting)

# Webserver für Konfiguration und Kalibrierung
def start_webserver():
    global server
    try:
        print("Initialisiere Webserver...")
        pool = socketpool.SocketPool(wifi.radio)
        server = Server(pool, "/www")
        print("Webserver-Instanz erstellt")

        # Minimales HTML mit Textfeld und Schaltfläche für Kalibrierung
        html = """
        <!DOCTYPE html>
        <html><head><title>FlapFlap Subclock</title>
        <style>body{font-family:Arial;margin:20px}h1{font-size:24px}h2{font-size:18px}label{display:block;margin:10px 0}input,select{margin:5px}button{padding:10px}</style>
        </head><body>
        <h1>FlapFlap Subclock</h1>
        <h2>Konfiguration</h2>
        <form id="configForm">
            <label>Nullpunkt: <input type="number" step="0.01" name="calibrated_zero_angle" id="zeroAngle"></label>
            <label>Typ: <select name="type" id="typeSelect" onchange="updateLeafInput()">
                <option value="Sekunden">Sekunden</option>
                <option value="Minuten">Minuten</option>
                <option value="Stunden">Stunden</option>
                <option value="nix62">Leerblatt (Sek/Min)</option>
                <option value="nix40">Leerblatt (Stunden)</option>
                <option value="Kalibrierung">Kalibrierung</option>
            </select></label>
            <label id="leafLabel" style="display:none">Zielblatt: <input type="number" name="calibration_leaf" id="calibrationLeaf" min="0"></label>
            <button type="button" onclick="saveConfig()">Speichern</button>
        </form>
        <h2>Kalibrierung</h2>
        <button onclick="calibrate()">Kalibrieren</button>
        <button onclick="advanceLeaf()" id="advanceButton" style="display:none">Blatt weiter</button>
        <script>
            function updateLeafInput(){
                let type=document.getElementById('typeSelect').value;
                let leafInput=document.getElementById('calibrationLeaf');
                let leafLabel=document.getElementById('leafLabel');
                let advanceButton=document.getElementById('advanceButton');
                if(type==='Kalibrierung'){
                    leafLabel.style.display='block';
                    advanceButton.style.display='inline';
                    leafInput.max=type==='Stunden'||type==='nix40'?'39':'61';
                }else{
                    leafLabel.style.display='none';
                    advanceButton.style.display='none';
                    leafInput.value='0';
                }
            }
            async function saveConfig(){
                let form=document.getElementById('configForm');
                let data={
                    calibrated_zero_angle:parseFloat(form.calibrated_zero_angle.value),
                    type:form.type.value,
                    calibration_leaf:parseInt(form.calibration_leaf.value)
                };
                let response=await fetch('/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});
                alert(await response.text());
                updateLeafInput();
            }
            async function calibrate(){
                let response=await fetch('/calibrate',{method:'POST'});
                alert(await response.text());
            }
            async function advanceLeaf(){
                let response=await fetch('/advance_leaf',{method:'POST'});
                alert(await response.text());
            }
            async function loadConfig(){
                let response=await fetch('/config');
                let data=await response.json();
                document.getElementById('zeroAngle').value=data.calibrated_zero_angle;
                document.getElementById('typeSelect').value=data.type;
                document.getElementById('calibrationLeaf').value=data.calibration_leaf;
                updateLeafInput();
            }
            loadConfig();
        </script>
        </body></html>
        """

        @server.route("/", "GET")
        def serve_index(request: Request):
            print("Verarbeite Anfrage: /")
            return Response(request, html, content_type="text/html")

        @server.route("/config", "GET")
        def get_config(request: Request):
            print("Verarbeite Anfrage: /config GET")
            return Response(request, json.dumps({
                "calibrated_zero_angle": calibrated_zero_angle,
                "type": type,
                "calibration_leaf": calibration_leaf
            }))

        @server.route("/config", "POST")
        def set_config(request: Request):
            global calibrated_zero_angle, type, calibration_leaf
            try:
                print("Verarbeite Anfrage: /config POST")
                data = json.loads(request.body)
                calibrated_zero_angle = float(data.get("calibrated_zero_angle", calibrated_zero_angle))
                type = data.get("type", type)
                calibration_leaf = int(data.get("calibration_leaf", calibration_leaf))
                # Validierung des calibration_leaf
                max_leaf = 39 if type in ["Stunden", "nix40"] else 61
                if calibration_leaf < 0 or calibration_leaf > max_leaf:
                    calibration_leaf = DEFAULT_CALIBRATION_LEAF
                    print(f"Ungültiger Blattwert, zurückgesetzt auf {DEFAULT_CALIBRATION_LEAF}")
                save_config()
                return Response(request, "Konfiguration aktualisiert!")
            except Exception as e:
                log_error(f"Konfigurations-Update-Fehler: {e}")
                return Response(request, f"Fehler: {e}", status=400)

        @server.route("/calibrate", "POST")
        def web_calibrate(request: Request):
            print("Verarbeite Anfrage: /calibrate POST")
            calibrate_zero_point()
            return Response(request, "Kalibrierung durchgeführt!")

        @server.route("/advance_leaf", "POST")
        def web_advance_leaf(request: Request):
            print("Verarbeite Anfrage: /advance_leaf POST")
            advance_leaf()
            return Response(request, "Blatt weitergedreht!")

        if wifi.radio.ipv4_address:
            server.start(host=str(wifi.radio.ipv4_address), port=8080)
            print(f"Webserver läuft unter {wifi.radio.ipv4_address}:8080")
        else:
            print("Kein WLAN, Webserver nicht gestartet.")
            log_error("Kein WLAN für Webserver")
    except Exception as e:
        print(f"Webserver-Fehler: {e}")
        log_error(f"Webserver-Fehler: {e}")

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
                print(f"WLAN verbunden! IP: {wifi.radio.ipv4_address}")
                return True
            else:
                print("WLAN-Zugangsdaten nicht gefunden.")
                log_error("WLAN-Zugangsdaten nicht gefunden")
    except Exception as e:
        print(f"Fehler beim Laden der WLAN-Konfiguration: {e}")
        log_error(f"WLAN-Fehler: {e}")
    return False

# Funktion zur Aktualisierung der Anzeige
def update_display():
    global step_target
    if type == "Kalibrierung":
        step_target = calibration_leaf
    elif type == "nix62":
        step_target = LEERBLATT_SEKUNDEN_MINUTEN
    elif type == "nix40":
        step_target = LEERBLATT_STUNDEN
    try:
        magnetic = average_magnetic_field(sensor)
        magnetic = average_magnetic_field(sensor)
        angle = calculate_rotation(magnetic[0], magnetic[1])
        step = calculate_step(angle, calibrated_zero_angle)
        if step == step_target:
            pin.value = False
            if type != "Kalibrierung":
                uart_read()
            time.sleep(0.1 if type == "Sekunden" else 0.5)
        else:
            pin.value = True
            print(f"Winkel: {angle:.2f}°, Aktuelle Stufe: {step}, Zielstufe: {step_target}")
            time.sleep(0.01)
        return step
    except ValueError as e:
        print(f"Sensorfehler: {e}")
        log_error(f"Sensorfehler: {e}")
        return step_target

# Hauptprogramm
try:
    sys.stdout.write("╔════════════════════════════════════════╗\r")
    sys.stdout.write("║                                        ║\r")
    sys.stdout.write("║       ⏰ FlapFlap Version 1.0.0        ║\r")
    sys.stdout.write("║           Subclock Software            ║\r")
    sys.stdout.write("║   (c) eHaJo, 2024, Twitch-Livestream   ║\r")
    sys.stdout.write("║     Projekt - https://www.eHaJo.de     ║\r")
    sys.stdout.write("║                                        ║\r")
    sys.stdout.write("╚════════════════════════════════════════╝\r\r")

    load_config()
    connect_to_wifi()
    start_webserver()
    running = True

    flush_uart_input()
    step = update_display()

    while True:
        if server:
            server.poll()  # Webserver-Anfragen verarbeiten
        if not button.value:
            calibrate_zero_point()
            time.sleep(0.5)
        step = update_display()

except Exception as e:
    print(f"Hauptprogrammfehler: {e}")
    log_error(f"Hauptprogrammfehler: {e}")
    while True:
        time.sleep(1)  # Verhindert Neustartschleife