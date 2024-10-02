import board
import busio
import adafruit_tlv493d
import math
import time
import json
import socketpool
import wifi

# I2C setup für den TLV493D-A1B6
i2c = busio.I2C(board.GP17, board.GP16)  # SCL = GP17, SDA = GP16
sensor = adafruit_tlv493d.TLV493D(i2c)
sensor.fast_mode = True  # Setze den Sensor in den Fast Mode

# WLAN-Zugangsdaten aus config.json lesen
def read_config():
    with open('config.json') as f:
        config = json.load(f)
    return config['ssid'], config['password']

# WLAN verbinden
def connect_to_wifi(ssid, password):
    wifi.radio.connect(ssid, password)
    print("Verbunden:", wifi.radio.ipv4_address)

# Funktion zur Berechnung des Rotationswinkels in Grad
def calculate_rotation(x, y):
    angle_radians = math.atan2(y, x)
    angle_degrees = math.degrees(angle_radians)
    return angle_degrees + 360 if angle_degrees < 0 else angle_degrees

# Erstellen eines einfachen Webservers
def start_webserver():
    pool = socketpool.SocketPool(wifi.radio)
    addr = str(wifi.radio.ipv4_address)  # IP-Adresse als String abrufen
    server_socket = pool.socket(pool.AF_INET, pool.SOCK_STREAM)
    server_socket.bind((addr, 80))
    server_socket.listen(1)
    print("Webserver läuft auf http://{}:80".format(addr))
    return server_socket

# Funktion zum Erstellen der HTML-Seite
def generate_html(angle):
    return f"""
    <!DOCTYPE html>
    <html lang="de">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Rotationsanzeige</title>
        <style>
            body {{ display: flex; justify-content: center; align-items: center; height: 100vh; background-color: #f0f0f0; }}
            h1 {{ font-size: 2em; }}
        </style>
    </head>
    <body>
        <h1>Aktuelle Drehlage: {angle:.2f}°</h1>
    </body>
    </html>
    """

# WLAN-Zugangsdaten lesen und verbinden
ssid, password = read_config()
connect_to_wifi(ssid, password)

# Hauptprogramm
server_socket = start_webserver()

while True:
    magnetic = sensor.magnetic  # Magnetfeld-Daten vom Sensor lesen
    x, y = magnetic[0], magnetic[1]

    angle = calculate_rotation(x, y)  # Rotationswinkel berechnen

    # Warten auf eine eingehende Verbindung
    try:
        client_socket, addr = server_socket.accept()
        print('Client verbunden von', addr)

        request = client_socket.recv(1024).decode('utf-8')  # Eingabedaten lesen
        if request.startswith('GET /'):
            # HTML-Seite zurückgeben
            html = generate_html(angle)
            client_socket.send(b'HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n')
            client_socket.send(html.encode('utf-8'))

    except Exception as e:
        print("Fehler bei der Bearbeitung der Anfrage:", e)
    finally:
        client_socket.close()

    time.sleep(0.02)  # Abfrage alle 20 ms (50 Mal pro Sekunde)
