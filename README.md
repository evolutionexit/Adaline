# Adaline — Remote OS Installation via Hardware HID Injection

> Remotely install an operating system on a dead machine using a Raspberry Pi Pico W, a Raspberry Pi 5, and a web dashboard — no screen capture, no prior software on the target machine.

---

## What is this?

Adaline lets you take full control of a machine's keyboard remotely, even before an OS is running. A Raspberry Pi Pico W plugged into the target machine emulates a USB HID keyboard. A web dashboard lets you send keystrokes and macro sequences from anywhere in the world. A Raspberry Pi 5 bridges the two, translating dashboard commands into UDP packets the Pico W executes.

**Typical use case:** a family member's PC is completely dead. You mail them a Pico W, they plug it in, and you walk the machine through a full OS install from your browser — no phone support, no USB drive, no technical knowledge required on their end.

---

## Architecture

```
Browser (pi.mmoors.me)
        │
        │  MQTT over WebSocket (wss://mqtt.mmoors.me)
        ▼
Cloudflare Tunnel
        │
        ▼
Raspberry Pi 5
  • Mosquitto MQTT broker (WebSocket on port 9001)
  • Python bridge script
    - Subscribes to MQTT topics
    - Translates to HID keycodes
    - Sends UDP packets with ACK
        │
        │  UDP (custom lightweight protocol)
        ▼
Raspberry Pi Pico W
  • C firmware (Pico SDK + TinyUSB + lwIP)
  • Emulates USB HID keyboard
  • Sends ACK back to RPi 5
        │
        │  USB HID
        ▼
  Target Machine
  (dead, no OS, BIOS level)
```

The dashboard is hosted on Cloudflare Pages at `pi.mmoors.me`. It connects to the RPi 5's Mosquitto broker via a **Cloudflare Tunnel** exposed at `mqtt.mmoors.me` — no port forwarding required, works behind any router or NAT. In local development it connects directly to the RPi 5 at `ws://192.168.0.11:9001`.

---

## Repository Structure

```
Adaline/
├── pico/                     # Raspberry Pi Pico W firmware (C)
│   ├── stacking.c                # Main logic: WiFi, UDP, HID injection
│   ├── usb_descriptors.c         # TinyUSB HID descriptor setup
│   ├── usb_descriptors.h
│   ├── tusb_config.h             # TinyUSB configuration
│   ├── lwipopts.h                # lwIP (UDP stack) configuration
│   ├── wifi_config.h             # WiFi credentials (not committed)
│   ├── wifi_config_example.h
│   ├── CMakeLists.txt
│   └── pico-sdk/                 # Pico SDK submodule
│
├── rpi/                      # Raspberry Pi 5 bridge script (Python)
│   └── bridge.py                 # MQTT subscriber → HID keycode → UDP sender
│
├── dashboard/                # Web dashboard (React + Vite + TypeScript)
│   ├── src/
│   │   └── App.tsx               # Main UI: text input, shortcut sender, macro buttons
│   ├── public/
│   └── ...
│
└── README.md
```

---

## How It Works

### 1. Pico W (C firmware)
The Pico W presents itself to the target machine as a standard USB HID keyboard using TinyUSB. It connects to the local WiFi network and listens for UDP packets from the RPi 5. Each packet contains a modifier byte and a keycode byte. On receipt, the Pico W injects the keystroke into the target machine and sends back an ACK packet.

### 2. RPi 5 Bridge (Python)
The bridge script connects to a local Mosquitto MQTT broker and subscribes to three topics:

| Topic | Purpose |
|---|---|
| `adaline/keyboard/text` | Type a string character by character |
| `adaline/keyboard/shortcut` | Send a key combination (e.g. `CTRL+ALT+DELETE`) |
| `adaline/keyboard/layout` | Switch keyboard layout at runtime |

When a message arrives, the script translates it into raw HID keycodes using a layout table, then sends a 2-byte UDP packet `[modifier, keycode]` to the Pico W and waits for an ACK. Round-trip time is typically 10–20ms on a local network. If the ACK times out (router drops the packet), execution continues without confirmation — a known limitation.

Supported layouts: Swiss German (`sg`). QWERTY, AZERTY, and Latin American Spanish are stubs.

### 3. Dashboard (React + Vite + TypeScript)
A minimal browser-based control panel hosted at `pi.mmoors.me`. Features:
- Connection status indicator (green/red)
- Free text input — sends character by character via `adaline/keyboard/text`
- Shortcut input — sends key combos via `adaline/keyboard/shortcut` (e.g. `CTRL+ALT+DELETE`, `GUI+R`)
- Hardcoded macro buttons (WIN+R, ENTER) that pre-fill the input for confirmation before sending

Connects to `wss://mqtt.mmoors.me` in production and `ws://192.168.0.11:9001` in local development.

---

## Hardware Requirements

| Component | Purpose |
|---|---|
| Raspberry Pi Pico W | USB HID keyboard emulator, plugged into target machine |
| Raspberry Pi 5 | MQTT broker + bridge script |
| Target machine | Any PC/laptop with a free USB-A port |
| Network | RPi 5 and Pico W on the same WiFi network, RPi 5 with internet access for Cloudflare Tunnel |

---

## Getting Started

### Pico W Firmware

```bash
# Clone the repo with submodules
git clone --recurse-submodules https://github.com/evolutionexit/Adaline
cd Adaline/pico

# Copy and fill in WiFi credentials
cp wifi_config_example.h wifi_config.h
# Edit wifi_config.h with your SSID and password

# Build
mkdir build && cd build
cmake ..
make

# Flash: hold BOOTSEL, plug in USB, drag .uf2 to the RPI-RP2 drive
```

### RPi 5 Bridge

```bash
# Install Mosquitto
sudo apt install mosquitto mosquitto-clients
sudo systemctl enable mosquitto
sudo systemctl start mosquitto

# Install Python dependency
pip install paho-mqtt

# Run the bridge
cd Adaline/rpi
python bridge.py
```

### Dashboard

```bash
cd Adaline/dashboard
npm install
npm run dev
# → http://localhost:5173 (connects to RPi 5 at ws://192.168.0.11:9001)
```

For production, deploy to Cloudflare Pages and set up a Cloudflare Tunnel on the RPi 5 pointing to `localhost:9001` for the MQTT WebSocket endpoint.

---

## Known Limitations

- **No video capture** — the Pico W cannot capture the target machine's screen. You must know the keystroke sequence for the install process in advance, or have someone on-site describe the screen.
- **ACK timeout fallback** — if the router drops the ACK UDP packet, the RPi 5 continues without confirmation. A keepalive + sequence number system is planned.
- **BIOS variability** — different manufacturers use different keys to access boot menus. A macro library per manufacturer is planned but not yet implemented.
- **OS image delivery** — the user currently needs to separately create a bootable USB. Netboot/PXE integration via `netboot.xyz` is being researched.
- **Keyboard layouts** — only Swiss German (`sg`) is fully implemented.

---

## Roadmap

- [ ] Keepalive packets to prevent router UDP timeout
- [ ] Sequence numbers + retransmit requests for reliable delivery
- [ ] BIOS macro library (AMI, Phoenix, Dell, HP, Lenovo)
- [ ] OS install wizard (Windows / Linux, clean install / dual boot)
- [ ] Netboot/PXE integration via netboot.xyz
- [ ] Full macro button system on the dashboard (replace free-text input)
- [ ] Additional keyboard layouts (QWERTY, AZERTY, Latin American Spanish)
- [ ] Consumer control keys (media keys, brightness, etc.)

---

## Tech Stack

| Layer | Technology |
|---|---|
| Pico W firmware | C, Pico SDK, TinyUSB, lwIP |
| RPi 5 bridge | Python, paho-mqtt |
| MQTT broker | Mosquitto |
| Dashboard | React 18, Vite, TypeScript |
| Hosting | Cloudflare Pages |
| Tunnel | Cloudflare Tunnel |

---

## Status

Early-stage working prototype. The full chain (dashboard → MQTT → Cloudflare Tunnel → RPi 5 → UDP → Pico W → USB HID → target machine) is functional. Active development ongoing at a slow pace alongside university studies.

---

## Author

Michel Moors — [mmoors.me](https://mmoors.me)
