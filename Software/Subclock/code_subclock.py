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

MOTOR_PULSE_DURATION = 0.04
MOTOR_SETTLE_TIME = 0.02
STABLE_READS_REQUIRED = 2
SENSOR_SAMPLES = 3
MAX_PULSES_PER_UPDATE = 10
MAX_UNCHANGED_STEP_COUNT = 3

DISPLAY_MOVE_TIMEOUT = 6.0
MAX_FAILED_MOVE_CYCLES = 61

BUTTON_DEBOUNCE_TIME = 0.5

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
pin.value = False

# Taste an GPIO22 mit internem Pull-up-Widerstand
button = digitalio.DigitalInOut(board.GP22)
button.direction = digitalio.Direction.INPUT
button.pull = digitalio.Pull.UP

# Variablen
running = False
calibrated_zero_angle = DEFAULT_ZERO_ANGLE
display_type = DEFAULT_TYPE
calibration_leaf = DEFAULT_CALIBRATION_LEAF
step_target = 0
abort_step_target = False
fatal_error = False
failed_move_cycles = 0
last_button_press_time = 0


# Logfunktion
def log_error(message):
    try:
        if storage.getmount("/").readonly:
            storage.remount("/", readonly=False, disable_concurrent_write_protection=True)
        with open("/error.log", "a") as f:
            f.write(f"{time.localtime()} - {message}\n")
    except Exception as e:
        print(f"Logfehler: {e}")
    finally:
        try:
            if not storage.getmount("/").readonly:
                storage.remount("/", readonly=True)
        except Exception:
            pass


# Funktion zur Berechnung des Rotationswinkels
def calculate_rotation(x, y):
    angle_radians = math.atan2(y, x)
    angle_degrees = math.degrees(angle_radians)
    return angle_degrees + 360 if angle_degrees < 0 else angle_degrees


# Funktion zur Berechnung der Stufe auf Basis des Winkels und Nullpunkts
def calculate_step(angle, zero_angle):
    pages = PAGES_SEKUNDEN_MINUTEN if display_type in ["Sekunden", "Minuten", "nix62", "Kalibrierung"] else PAGES_STUNDEN
    adjusted_angle = (zero_angle - angle) % 360
    step = int(adjusted_angle / (360 / pages))

    if step < 0:
        step = 0
    if step >= pages:
        step = pages - 1

    return step


# Funktion zur Mittelung von Sensorwerten
def average_magnetic_field(sensor, num_samples=3, dummy_read=False):
    if dummy_read:
        sensor.magnetic

    total_x, total_y = 0, 0

    for _ in range(num_samples):
        magnetic = sensor.magnetic
        if magnetic[0] == 0 and magnetic[1] == 0:
            raise ValueError("Ungültige Sensordaten")
        total_x += magnetic[0]
        total_y += magnetic[1]

    return total_x / num_samples, total_y / num_samples


# Aktuelle Stufe lesen
def read_current_step(num_samples=SENSOR_SAMPLES):
    magnetic = average_magnetic_field(sensor, num_samples=num_samples, dummy_read=True)
    angle = calculate_rotation(magnetic[0], magnetic[1])
    step = calculate_step(angle, calibrated_zero_angle)
    return step, angle


# Funktion zum Laden der Konfiguration
def load_config():
    global calibrated_zero_angle, display_type, calibration_leaf
    try:
        with open("/config.json", "r") as f:
            config = json.load(f)
            calibrated_zero_angle = config.get("calibrated_zero_angle", DEFAULT_ZERO_ANGLE)
            display_type = config.get("type", DEFAULT_TYPE)
            calibration_leaf = config.get("calibration_leaf", DEFAULT_CALIBRATION_LEAF)

            print(f"Konfiguration geladen: Nullpunkt = {calibrated_zero_angle}, Typ = {display_type}, Kalibrierungsblatt = {calibration_leaf}")

            if display_type == "Sekunden":
                print("Ich bin eine Sekundenanzeige!")
            elif display_type == "Minuten":
                print("Ich bin eine Minutenanzeige!")
            elif display_type == "Kalibrierung":
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
        "type": display_type,
        "calibration_leaf": calibration_leaf
    }

    try:
        storage.remount("/", readonly=False, disable_concurrent_write_protection=True)
        with open("config.json", "w") as f:
            json.dump(config, f)
        print("Konfiguration gespeichert!")
    except Exception as e:
        print(f"Fehler beim Speichern der Konfiguration: {e}")
        log_error(f"Konfigurationsspeicherfehler: {e}")
    finally:
        try:
            storage.remount("/", readonly=True)
        except Exception:
            pass


# Nullpunktkalibrierung
def calibrate_zero_point():
    global calibrated_zero_angle
    try:
        magnetic = average_magnetic_field(sensor, num_samples=10, dummy_read=True)
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
        print("Motor gepulst (Blatt weiter)")
    except Exception as e:
        print(f"Fehler beim Vorwärtsbewegen: {e}")
        log_error(f"Fehler beim Vorwärtsbewegen: {e}")


# UART lesen und Ziel aus seriellen Daten ableiten
def uart_read():
    global step_target

    data = uart.read(32)
    if data and display_type != "Kalibrierung":
        if len(data) >= 3:
            if display_type == "Sekunden":
                step_target = data[0]
                if step_target > 61:
                    step_target = 0
                if step_target > 30:
                    step_target += 1

            elif display_type == "Minuten":
                step_target = data[1]
                if step_target > 61:
                    step_target = 0
                if step_target > 30:
                    step_target += 1

            elif display_type == "Stunden":
                step_target = data[2]
                if step_target > 39:
                    step_target = 0

            elif display_type == "nix62":
                step_target = LEERBLATT_SEKUNDEN_MINUTEN

            elif display_type == "nix40":
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
                "type": display_type,
                "calibration_leaf": calibration_leaf
            }))

        @server.route("/config", "POST")
        def set_config(request: Request):
            global calibrated_zero_angle, display_type, calibration_leaf
            try:
                print("Verarbeite Anfrage: /config POST")
                data = json.loads(request.body)

                calibrated_zero_angle = float(data.get("calibrated_zero_angle", calibrated_zero_angle))
                display_type = data.get("type", display_type)
                calibration_leaf = int(data.get("calibration_leaf", calibration_leaf))

                max_leaf = 39 if display_type in ["Stunden", "nix40"] else 61
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


# Zielblatt bestimmen
def determine_step_target():
    global step_target

    if display_type == "Kalibrierung":
        return calibration_leaf

    if display_type == "nix62":
        return LEERBLATT_SEKUNDEN_MINUTEN

    if display_type == "nix40":
        return LEERBLATT_STUNDEN

    uart_read()
    return step_target


# Ein Regelzyklus
def move_one_cycle_to_target():
    global step_target, failed_move_cycles, fatal_error

    if fatal_error:
        pin.value = False
        return step_target

    step_target = determine_step_target()

    stable_hits = 0
    pulses = 0
    last_step = None
    unchanged_count = 0
    final_step = step_target
    move_start = time.monotonic()

    try:
        while pulses <= MAX_PULSES_PER_UPDATE:
            if time.monotonic() - move_start > DISPLAY_MOVE_TIMEOUT:
                pin.value = False
                failed_move_cycles += 1
                print(f"FEHLER: Stellvorgang Timeout ({failed_move_cycles}/{MAX_FAILED_MOVE_CYCLES})")
                log_error(f"Stellvorgang Timeout ({failed_move_cycles}/{MAX_FAILED_MOVE_CYCLES})")

                if failed_move_cycles >= MAX_FAILED_MOVE_CYCLES:
                    fatal_error = True
                    print("FATAL ERROR: Anzeige gestoppt, weil kein Blatt mehr sicher gefunden wird")
                    log_error("FATAL ERROR: Anzeige gestoppt, weil kein Blatt mehr sicher gefunden wird")

                return final_step

            step, angle = read_current_step()
            final_step = step

            if step == step_target:
                stable_hits += 1
                pin.value = False
                print(f"Winkel: {angle:.2f}°, Aktuelle Stufe: {step}, Zielstufe: {step_target}, stabile Treffer: {stable_hits}/{STABLE_READS_REQUIRED}")

                if stable_hits >= STABLE_READS_REQUIRED:
                    failed_move_cycles = 0
                    return final_step

                time.sleep(0.01)
                continue

            stable_hits = 0

            if last_step == step:
                unchanged_count += 1
            else:
                unchanged_count = 0

            last_step = step

            print(f"Winkel: {angle:.2f}°, Aktuelle Stufe: {step}, Zielstufe: {step_target}, Puls {pulses + 1}/{MAX_PULSES_PER_UPDATE}")

            if unchanged_count >= MAX_UNCHANGED_STEP_COUNT:
                pin.value = False
                failed_move_cycles += 1
                print(f"Warnung: Stufe ändert sich trotz Motorpuls nicht ({failed_move_cycles}/{MAX_FAILED_MOVE_CYCLES})")
                log_error(f"Mechanik blockiert oder Sensorwert steht ({failed_move_cycles}/{MAX_FAILED_MOVE_CYCLES})")

                if failed_move_cycles >= MAX_FAILED_MOVE_CYCLES:
                    fatal_error = True
                    print("FATAL ERROR: Anzeige gestoppt, weil kein Blatt mehr sicher gefunden wird")
                    log_error("FATAL ERROR: Anzeige gestoppt, weil kein Blatt mehr sicher gefunden wird")

                return final_step

            advance_leaf()
            pulses += 1
            time.sleep(MOTOR_SETTLE_TIME)

        pin.value = False
        failed_move_cycles += 1
        print(f"Warnung: Zielblatt in diesem Zyklus nicht sicher erreicht ({failed_move_cycles}/{MAX_FAILED_MOVE_CYCLES})")
        log_error(f"Zielblatt in Zyklus nicht erreicht ({failed_move_cycles}/{MAX_FAILED_MOVE_CYCLES})")

        if failed_move_cycles >= MAX_FAILED_MOVE_CYCLES:
            fatal_error = True
            print("FATAL ERROR: Anzeige gestoppt, weil kein Blatt mehr sicher gefunden wird")
            log_error("FATAL ERROR: Anzeige gestoppt, weil kein Blatt mehr sicher gefunden wird")

        return final_step

    except ValueError as e:
        pin.value = False
        failed_move_cycles += 1
        print(f"Sensorfehler: {e} ({failed_move_cycles}/{MAX_FAILED_MOVE_CYCLES})")
        log_error(f"Sensorfehler: {e} ({failed_move_cycles}/{MAX_FAILED_MOVE_CYCLES})")

        if failed_move_cycles >= MAX_FAILED_MOVE_CYCLES:
            fatal_error = True
            print("FATAL ERROR: Anzeige gestoppt wegen wiederholter Sensorfehler")
            log_error("FATAL ERROR: Anzeige gestoppt wegen wiederholter Sensorfehler")

        return final_step

    except Exception as e:
        pin.value = False
        failed_move_cycles += 1
        print(f"Regelungsfehler: {e} ({failed_move_cycles}/{MAX_FAILED_MOVE_CYCLES})")
        log_error(f"Regelungsfehler: {e} ({failed_move_cycles}/{MAX_FAILED_MOVE_CYCLES})")

        if failed_move_cycles >= MAX_FAILED_MOVE_CYCLES:
            fatal_error = True
            print("FATAL ERROR: Anzeige gestoppt wegen wiederholter Regelungsfehler")
            log_error("FATAL ERROR: Anzeige gestoppt wegen wiederholter Regelungsfehler")

        return final_step


# Funktion zur Aktualisierung der Anzeige
def update_display():
    return move_one_cycle_to_target()


# Funktion zur Verarbeitung der Taste
def handle_button():
    global last_button_press_time

    now = time.monotonic()
    if not button.value and (now - last_button_press_time) > BUTTON_DEBOUNCE_TIME:
        last_button_press_time = now
        calibrate_zero_point()


# Hauptprogramm
try:
    sys.stdout.write("╔════════════════════════════════════════╗\r")
    sys.stdout.write("║                                        ║\r")
    sys.stdout.write("║       ⏰ FlapFlap Version 1.2.0        ║\r")
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
            server.poll()

        handle_button()

        if fatal_error:
            pin.value = False
            time.sleep(0.2)
            continue

        step = update_display()

except Exception as e:
    pin.value = False
    print(f"Hauptprogrammfehler: {e}")
    log_error(f"Hauptprogrammfehler: {e}")
    while True:
        time.sleep(1)