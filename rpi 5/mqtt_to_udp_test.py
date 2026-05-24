# TO DO : add consumer control keys handling


import paho.mqtt.client as mqtt
import socket
import time

MQTT_HOST = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC_TEXT = "adaline/keyboard/text"
MQTT_TOPIC_SHORTCUT = "adaline/keyboard/shortcut"
MQTT_TOPIC_LAYOUT = "adaline/keyboard/layout"
UDP_HOST = "192.168.1.38"
UDP_PORT = 6002
UDP_LISTEN_PORT = 6001
UDP_TIMEOUT = 0.01   

MOD_NONE  = 0x00
MOD_CTRL  = 0x01
MOD_SHIFT = 0x02
MOD_ALT   = 0x04
MOD_GUI   = 0x08

current_layout = "sg"

# Need to add layouts for QWERTY, AZERTY and Latin Amercian Spanish
LAYOUTS = {
    "sg": {
        # Letters
        'a': (0x04, 0x00), 'b': (0x05, 0x00), 'c': (0x06, 0x00), 'd': (0x07, 0x00), 'e': (0x08, 0x00), 'f': (0x09, 0x00),
        'g': (0x0A, 0x00), 'h': (0x0B, 0x00), 'i': (0x0C, 0x00), 'j': (0x0D, 0x00), 'k': (0x0E, 0x00), 'l': (0x0F, 0x00),
        'm': (0x10, 0x00), 'n': (0x11, 0x00), 'o': (0x12, 0x00), 'p': (0x13, 0x00), 'q': (0x14, 0x00), 'r': (0x15, 0x00),
        's': (0x16, 0x00), 't': (0x17, 0x00), 'u': (0x18, 0x00), 'v': (0x19, 0x00), 'w': (0x1A, 0x00), 'x': (0x1B, 0x00),
        'y': (0x1D, 0x00), 'z': (0x1C, 0x00),
        
        # Shifted letters
        'A': (0x04, 0x02), 'B': (0x05, 0x02), 'C': (0x06, 0x02), 'D': (0x07, 0x02), 'E': (0x08, 0x02), 'F': (0x09, 0x02),
        'G': (0x0A, 0x02), 'H': (0x0B, 0x02), 'I': (0x0C, 0x02), 'J': (0x0D, 0x02), 'K': (0x0E, 0x02), 'L': (0x0F, 0x02),
        'M': (0x10, 0x02), 'N': (0x11, 0x02), 'O': (0x12, 0x02), 'P': (0x13, 0x02), 'Q': (0x14, 0x02), 'R': (0x15, 0x02),
        'S': (0x16, 0x02), 'T': (0x17, 0x02), 'U': (0x18, 0x02), 'V': (0x19, 0x02), 'W': (0x1A, 0x02), 'X': (0x1B, 0x02),
        'Y': (0x1D, 0x02), 'Z': (0x1C, 0x02),

        # Numbers
        '1': (0x1E, 0x00), '2': (0x1F, 0x00), '3': (0x20, 0x00), '4': (0x21, 0x00), '5': (0x22, 0x00), '6': (0x23, 0x00),
        '7': (0x24, 0x00), '8': (0x25, 0x00), '9': (0x26, 0x00), '0': (0x27, 0x00),

        # Shifted numbers
        '+': (0x1E, 0x02), '"': (0x1F, 0x02), '*': (0x20, 0x02), 'ç': (0x21, 0x02), '%': (0x22, 0x02), '&': (0x23, 0x02),
        '/': (0x24, 0x02), '(': (0x25, 0x02), ')': (0x26, 0x02), '=': (0x27, 0x02),

        # Space
        " ": (0x2C, 0x00),

        # Other Characters
        "'": (0x2D, 0x00), '^': (0x2E, 0x00), 'ü': (0x2F, 0x00), '¨': (0x30, 0x00), 'ö': (0x33, 0x00), 'ä': (0x34, 0x00),
        '$': (0x31, 0x00), ',': (0x36, 0x00), '.': (0x37, 0x00), '-': (0x38, 0x00), '§': (0x35, 0x00), '<':(0x64, 0x00),

        # Other shifted characters
        '?': (0x2D, 0x02), '`': (0x2E, 0x02), 'è': (0x2F, 0x02), '!': (0x30, 0x02), 'é': (0x33, 0x02), 'à': (0x34, 0x02),
        '£': (0x31, 0x02), ';': (0x36, 0x02), ':': (0x37, 0x02), '_': (0x38, 0x02), '°': (0x35, 0x02), '>':(0x64, 0x02),

        # Other alted characters
        '¦': (0x1E, 0x04), '@': (0x1F, 0x04), '#': (0x20, 0x04), '¬': (0x23, 0x04), '|': (0x24, 0x04), '¢': (0x25, 0x04),
        '´': (0x2D, 0x04), '~': (0x2E, 0x04), '€': (0x08, 0x04), '[': (0x2F, 0x04), ']': (0x30, 0x04), '}': (0x31, 0x04),
        '\\': (0x64, 0x04),
    },
    "qwerty": {
        #  Add layout for qwerty
    }, 
    "azerty": {
        #  Add Layout for azerty
    },
    "LaAmSp": {
        #  Add layout for Latin American Spanish
    },

    # Maybe add other layouts
}

# Stays the same across all layouts
SPECIAL_KEYS = {
    'ENTER':     0x28,
    'ESC':       0x29,
    'BACKSPACE': 0x2A,
    'TAB':       0x2B,
    'DELETE':    0x4C,
    'INSERT':    0x49,
    'HOME':      0x4A,
    'END':       0x4D,
    'PAGEUP':    0x4B,
    'PAGEDOWN':  0x4E,
    'UP':        0x52,
    'DOWN':      0x51,
    'LEFT':      0x50,
    'RIGHT':     0x4F,
    'F1':        0x3A, 'F2':  0x3B, 'F3':  0x3C, 'F4':  0x3D,
    'F5':        0x3E, 'F6':  0x3F, 'F7':  0x40, 'F8':  0x41,
    'F9':        0x42, 'F10': 0x43, 'F11': 0x44, 'F12': 0x45,

}

udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_sock.bind(("0.0.0.0", UDP_LISTEN_PORT))
udp_sock.settimeout(UDP_TIMEOUT)



def send_raw_hid(modifier, keycode):
    packet = bytes([modifier, keycode])
    t1 = time.time()
    udp_sock.sendto(packet, (UDP_HOST, UDP_PORT))
    try:
        data, addr = udp_sock.recvfrom(64)
        t2 = time.time()
        print(f"RTT: {(t2-t1)*1000:.1f}ms")
    except socket.timeout:
        print("TIMEOUT")


def type_text(text):
    keycodes =LAYOUTS[current_layout]
    for char in normalize(text):
        if char in keycodes:
            keycode, modifier = keycodes[char]
            send_raw_hid(modifier, keycode)
        else:
            print(f"Skipping unknown character: '{char}' (U+{ord(char):04X})")

def normalize(text):
    return (text
        .replace('\u00A0', '')  # non-breaking space → nothing
        .replace('"', '"').replace('"', '"')
        .replace('»', '"').replace('«', '"')
        .replace('\u2018', "'").replace('\u2019', "'")  # use unicode escapes to be safe
        .replace('—', '--').replace('–', '-')
        .replace('…', '...')
    )

def on_connect(client, userdata, flags, rc, properties=None):
    print(f"Connected to MQTT broker (rc={rc})")
    client.subscribe(MQTT_TOPIC_TEXT)
    client.subscribe(MQTT_TOPIC_SHORTCUT)
    client.subscribe(MQTT_TOPIC_LAYOUT)
    print(f"Subscribed to {MQTT_TOPIC_TEXT}")
    print(f"Subscribed to {MQTT_TOPIC_SHORTCUT}")
    print(f"Subscribed to {MQTT_TOPIC_LAYOUT}")

def on_message(client, userdata, msg):
    print(f"RAW: topic={msg.topic} payload={msg.payload}")
    payload = msg.payload.decode("utf-8").strip()
    
    if msg.topic == MQTT_TOPIC_SHORTCUT:
        keycodes = LAYOUTS[current_layout]
        parts = payload.upper().split("+")
        mod = MOD_NONE
        key = 0x00
        for p in parts:
            if p == "CTRL": mod |= MOD_CTRL
            elif p == "ALT": mod |= MOD_ALT
            elif p == "SHIFT": mod |= MOD_SHIFT
            elif p == "GUI": mod |= MOD_GUI
            elif p in SPECIAL_KEYS: key = SPECIAL_KEYS[p]
            
            
            elif p.lower() in keycodes: key, _ = keycodes[p.lower()]
        
        if key != 0:
            print(f"Sending Combo: {payload}")
            send_raw_hid(mod, key)

    elif msg.topic == MQTT_TOPIC_TEXT:
        print(f"Typing Sentence: {payload}")
        type_text(payload)

    elif msg.topic == MQTT_TOPIC_LAYOUT:
        global current_layout
        layout = payload.lower()
        if layout in LAYOUTS:
            current_layout = layout
            print(f"Layout changed to: {current_layout}")
        else:
            print(f"Unknown layout: {layout}")


client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message
client.connect(MQTT_HOST, MQTT_PORT, 60)
client.loop_forever()