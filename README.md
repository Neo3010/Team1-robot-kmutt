# 🤖 ESP32 BLE Robot — KMUTT

A dual-ESP32 WiFi + Bluetooth robot with live camera streaming, BLE motor control, humidity monitoring, and a single-file browser interface. No app install required — just open the HTML file in Chrome.

> 🔗 **Circuit Designer Reference:** [View on Cirkit Designer](https://app.cirkitdesigner.com/project/c99521b6-accb-4e8c-8139-b703be0d8ee7)

---

## 📋 Table of Contents

- [How It Works](#how-it-works)
- [Repository Structure](#repository-structure)
- [Components](#components)
- [Arduino IDE Setup](#arduino-ide-setup)
- [Configuration Guide](#configuration-guide)
  - [ESP32 Motor — Things You Must Change](#1-esp32-motor--things-you-must-change)
  - [ESP32 Camera — Things You Must Change](#2-esp32-camera--things-you-must-change)
  - [Web Interface — Things You Must Change](#3-web-interface--things-you-must-change)
- [Wiring Reference](#wiring-reference)
- [Flashing the Boards](#flashing-the-boards)
- [Using the Web Interface](#using-the-web-interface)
- [BLE Protocol](#ble-protocol)
- [Troubleshooting](#troubleshooting)

---

## How It Works

The system uses two separate ESP32 boards that never talk to each other directly:

- **ESP32 Motor board** — listens for BLE commands from the browser, drives two motors via the Maker Drive, reads a DHT11 sensor, and pushes humidity/temperature back to the browser every 2 seconds.
- **ESP32-CAM board** — connects to WiFi and streams live MJPEG video over HTTP to the browser.
- **Browser (Chrome)** — connects to both simultaneously: BLE to the motor board, HTTP to the camera. No server needed.

```
[Browser]
   │
   ├── BLE (Bluetooth) ──────────────► [ESP32 Motor Board]
   │                                        │
   │   ◄── H:humidity:temp (notify) ────────┤
   │                                        ├── Maker Drive → Left Motor
   │                                        ├── Maker Drive → Right Motor
   │                                        ├── DHT11 (humidity/temp)
   │                                        ├── LDR (light sensor)
   │                                        └── LED (auto light indicator)
   │
   └── HTTP WiFi stream ◄──────────── [ESP32-CAM Board]
                                           └── OV2640 Camera
```

---

## Repository Structure

```
Team1-robot-kmutt/
├── controlesp32/
│   └── controlesp32.ino            ← Flash to ESP32 WROOM-32
├── cameraesp32/
│   └── cameraesp32.ino             ← Flash to ESP32-CAM (AI-Thinker)
├── robot-controller-website/
│   └── robot-controller.html       ← Open in Chrome, no server needed
├── circuit/
│   └── circuit-diagram.png         ← Prototype wiring reference
├── LICENSE
└── README.md
```

---

## Components

These are the **exact components used in this project**. The code is written and tested specifically for these parts. If you substitute a component, read the notes carefully — some require code changes.

### Main Boards

| Component | Exact Model Used | Substitution Notes |
|-----------|-----------------|-------------------|
| Microcontroller (Motor) | **ESP32 WROOM-32 DevKit** (38-pin) | Any 38-pin ESP32 DevKit should work |
| Microcontroller (Camera) | **ESP32-CAM AI-Thinker** | Pin definitions in the code match AI-Thinker only — other models need different pin blocks |

### Motor System

| Component | Exact Model Used | Substitution Notes |
|-----------|-----------------|-------------------|
| Motor Driver | **Cytron Maker Drive (MX1508)** | Controlled by 4 PWM pins only — no enable pin. If you use L298N, you must add ENA/ENB wired to 5V |
| DC Motors | Any 3–6V DC gearmotor | 2 required (left and right wheels) |

> ⚠️ **Important — Maker Drive vs L298N:**
> The Maker Drive is different from a standard L298N. It has **no ENA/ENB enable pins** — motor speed and direction are controlled entirely through PWM on M1A, M1B, M2A, M2B. The code is written for this behaviour. If you use an L298N instead, you need to wire ENA and ENB to 5V and the existing code will still work, but always-on at full enable.

### Sensors & Indicators

| Component | Exact Model Used | Notes |
|-----------|-----------------|-------|
| Humidity & Temperature | **DHT11** | Uses Grove Temperature and Humidity Sensor library |
| Light Sensor | **LDR (GL5528 or similar)** | Any standard LDR — wired as a voltage divider with a 10KΩ resistor |
| Status LED | Any 5mm through-hole LED | Controlled by LDR reading — turns on in dark environments |

### Power

| Component | Used For |
|-----------|---------|
| 9V battery | Powers the ESP32 motor board (connected to VIN) |
| 4× AA battery pack | Powers the Maker Drive motor supply (one pack per motor side, or one shared pack) |

---

## Arduino IDE Setup

### Step 1 — Install ESP32 Board Support

1. Open Arduino IDE → **File → Preferences**
2. Paste into *Additional Boards Manager URLs*:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Go to **Tools → Board → Boards Manager**
4. Search `esp32` → install **esp32 by Espressif Systems** (version 2.x recommended)

### Step 2 — Install Required Libraries

Go to **Sketch → Include Library → Manage Libraries** and install:

| Library Name | Used In | How to Find |
|-------------|---------|-------------|
| `Grove Temperature And Humidity Sensor` | controlesp32 | Search "Grove DHT" in Library Manager |
| `ESP32 BLE Arduino` | controlesp32 | Already included in ESP32 core — no install needed |
| `esp_camera` | cameraesp32 | Already included in ESP32 core — no install needed |

---

## Configuration Guide

Before flashing either board, there are values in the code that **must be updated to match your own hardware and network**. The sections below walk through every value you need to know about — what it does, where it is, and what to change it to.

---

### 1. ESP32 Motor — Things You Must Change

Open `controlesp32/controlesp32.ino`.

---

#### 🔵 BLE Device Name

This is the name that appears when the browser scans for Bluetooth devices. If you run more than one robot on the same project, each board needs a unique name.

```cpp
BLEDevice::init("ESP32_Robot_KMUTT");
//              ^^^^^^^^^^^^^^^^^^^^
//              Change to any name you want, e.g. "MyRobot_01"
```

Also update the matching placeholder in the web interface (see section 3 below).

---

#### 🔵 Motor Pin Assignments

These four GPIO pins connect to the Maker Drive input terminals. Change them only if you wire the motor driver to different GPIO pins.

```cpp
const int M1A = 32;  // Left motor  — BACKWARD PWM signal → Maker Drive M1A
const int M1B = 33;  // Left motor  — FORWARD  PWM signal → Maker Drive M1B
const int M2A = 25;  // Right motor — FORWARD  PWM signal → Maker Drive M2A
const int M2B = 26;  // Right motor — BACKWARD PWM signal → Maker Drive M2B
```

> **If the robot moves backward when you press forward:** swap M1A and M1B for the left side, or M2A and M2B for the right side — not both.
>
> **If left and right are swapped:** swap the M1 pair with the M2 pair entirely.

---

#### 🔵 PWM Channels

Each motor pin is assigned a LEDC channel. Channels 4–7 are used to avoid conflicts with ESP32 internal peripherals. You can change these if you have a conflict, but they must be unique (0–15) and not used elsewhere in your project.

```cpp
#define CH_M1A 4   // LEDC channel for GPIO32
#define CH_M1B 5   // LEDC channel for GPIO33
#define CH_M2A 6   // LEDC channel for GPIO25
#define CH_M2B 7   // LEDC channel for GPIO26
```

---

#### 🔵 DHT11 Pin

```cpp
#define DHT_PIN 14
//               ^^
//               GPIO number your DHT11 DATA wire is connected to.
//               Must have a 10KΩ pull-up resistor to 3.3V on this pin.
```

---

#### 🔵 LDR, LED, and Light Threshold

```cpp
#define LDR_PIN   34    // GPIO for LDR voltage divider midpoint.
                        // Must be an ADC-capable pin (GPIO 32–39 on WROOM-32).

#define LED_PIN   12    // GPIO for status LED (via 220Ω resistor).
                        // Any digital output GPIO works.

#define THRESHOLD 1000  // ADC value (0–4095) below which LED turns ON.
                        // Lower = LED turns on only in very dark conditions.
                        // Higher = LED turns on in dim/moderate light.
                        // Adjust based on your LDR and lighting environment.
```

---

#### 🔵 Motor Speeds

These are the startup defaults. The browser speed slider overrides them at runtime via `V:xxx` commands.

```cpp
int MOTOR_SPEED = 200;  // Default driving speed. Range: 0–255.
                        // 200 = ~78% power. Reduce if robot is too fast
                        // or motors strain on startup.

int TURN_SPEED  = 80;   // Inner wheel speed during a curve turn.
                        // Set automatically to ~40% of MOTOR_SPEED when
                        // a V: speed command is received.
                        // Increase for sharper turns, decrease for wider arcs.
```

---

#### 🔵 Watchdog Timeout

```cpp
#define WATCHDOG_MS 300  // Milliseconds of silence before motors auto-stop.
                         //
                         // The browser sends a command every 100ms while a
                         // key is held down, so 300ms gives 3× tolerance.
                         //
                         // Do not set below 150ms — motors will stutter.
                         // Increase if you have unreliable BLE (e.g. 500ms).
```

---

### 2. ESP32 Camera — Things You Must Change

Open `cameraesp32/cameraesp32.ino`.

---

#### 🟠 WiFi Credentials ← Required

The camera connects to a WiFi hotspot. Your browser device must be connected to the **same network** to receive the stream.

```cpp
const char* ssid     = "San";        // ← Replace with your WiFi/hotspot name
const char* password = "222777111";  // ← Replace with your WiFi password
```

---

#### 🟠 Camera Resolution

Adjust to balance image quality vs. frame rate depending on your WiFi strength.

```cpp
config.frame_size = FRAMESIZE_QVGA;   // Our default: 320×240
```

| Constant | Resolution | Frame Rate | Best For |
|----------|-----------|-----------|---------|
| `FRAMESIZE_QQVGA` | 160×120 | ~30fps | Weak hotspot, smoothest motion |
| `FRAMESIZE_QVGA` | 320×240 | ~20fps | ✅ Recommended — good balance |
| `FRAMESIZE_VGA` | 640×480 | ~8fps | Strong WiFi, most detail |

---

#### 🟠 JPEG Quality

```cpp
config.jpeg_quality = 12;  // 10 = highest quality (larger frames, more bandwidth)
                            // 20 = lowest quality (smaller frames, less bandwidth)
                            // Recommended: 10–15
```

---

#### 🟠 Camera Pin Definitions

The full pin block at the top of the file is pre-configured for the **AI-Thinker ESP32-CAM**. Do not change these unless you are using a different board layout.

```cpp
// Only change if using a NON AI-Thinker ESP32-CAM board:
#define XCLK_GPIO_NUM  21
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
// ... (all other Y2–Y9, VSYNC, HREF, PCLK pins)
```

If you use a different ESP32-CAM model (M5Camera, Freenove, Wrover), replace the entire pin block with the definitions for your board. Espressif's `CameraWebServer` example contains pin blocks for all common models.

---

### 3. Web Interface — Things You Must Change

Open `robot-controller-website/robot-controller.html` in any text editor.

---

#### 🔵 Default Camera IP Address

The interface auto-connects to this IP on page load. Set it to your ESP32-CAM's IP (printed in Serial Monitor after the camera boots).

Find this line (near the bottom of the `<body>` section):

```html
<input id="cam-ip" type="text" value="http://172.20.10.2" ... />
```

Change `http://172.20.10.2` to your camera's actual IP. You can also just type it into the field in the browser without editing the file.

---

#### 🔵 BLE Device Name Placeholder

If you changed the BLE name in the motor firmware, update the placeholder here to match. This is just a UI hint — the user can type any name.

```html
<input id="bt-name" type="text" placeholder="ESP32_Robot_KMUTT" />
```

Change `ESP32_Robot_KMUTT` to whatever name you used in `BLEDevice::init(...)`.

---

## Wiring Reference

### ESP32 Motor Board → Maker Drive

| ESP32 GPIO | Maker Drive Terminal | Motor Side | Direction |
|------------|---------------------|-----------|----------|
| GPIO 32 | M1A | Left | BACKWARD |
| GPIO 33 | M1B | Left | FORWARD |
| GPIO 25 | M2A | Right | FORWARD |
| GPIO 26 | M2B | Right | BACKWARD |
| GND | GND | — | — |

The Maker Drive **PWR** terminal connects to your AA battery pack (separate from the ESP32's 9V supply).

### ESP32 Motor Board → DHT11

```
ESP32 3.3V ──┬──── DHT11 VCC  (pin 1)
             │
            10KΩ  ← pull-up resistor
             │
ESP32 GPIO14 ┴──── DHT11 DATA (pin 2)
                   DHT11 NC   (pin 3) ← leave unconnected
ESP32 GND   ────── DHT11 GND  (pin 4)
```

### ESP32 Motor Board → LDR

```
ESP32 3.3V
    │
   LDR  (light-dependent resistor)
    │
    ├──────── ESP32 GPIO 34  (analog read)
    │
   10KΩ
    │
ESP32 GND
```

### ESP32 Motor Board → LED

```
ESP32 GPIO 12 ──── 220Ω resistor ──── LED (+) anode
                                       LED (–) cathode ──── GND
```

### ESP32-CAM Power and Flashing

| ESP32-CAM Pin | Normal Operation | Flashing Mode |
|---------------|-----------------|---------------|
| 5V | 5V supply | 5V supply |
| GND | Ground | Ground |
| GPIO0 | Leave floating | Connect to GND |

---

## Flashing the Boards

### Flash the ESP32 Motor Board

1. Open `controlesp32/controlesp32.ino`
2. **Tools → Board** → `ESP32 Dev Module`
3. **Tools → Upload Speed** → `115200`
4. **Tools → Port** → select your COM port
5. Click **Upload**
6. Open **Serial Monitor** at `115200` baud — confirm output:
   ```
   =================================
     ESP32_Robot_KMUTT — BLE READY
     Motor Speed: 200/255
     DHT Pin    : GPIO14
     LDR Pin    : GPIO34
     LED Pin    : GPIO12
   =================================
   ```

### Flash the ESP32-CAM

The ESP32-CAM has no native USB port — you need a USB-to-TTL serial adapter (FTDI232, CH340, or CP2102).

**Wiring for flash mode:**

| USB-TTL Adapter | ESP32-CAM |
|-----------------|-----------|
| 5V | 5V |
| GND | GND |
| TX | GPIO3 (U0RXD) |
| RX | GPIO1 (U0TXD) |
| GND | **GPIO0** ← enables flash mode |

1. Wire GPIO0 to GND, then press the ESP32-CAM Reset button
2. **Tools → Board** → `AI Thinker ESP32-CAM`
3. **Tools → Port** → select your adapter's port
4. Click **Upload**
5. After upload: **disconnect GPIO0 from GND**, press Reset
6. Open **Serial Monitor** at `115200`:
   ```
   [WiFi] Connected!
   [CAM] Stream URL: http://172.20.10.2
   ```
   Copy this IP — you need it in the web interface.

---

## Using the Web Interface

Open `robot-controller-website/robot-controller.html` directly in **Google Chrome** or **Microsoft Edge**.

> ⚠️ Web Bluetooth only works in **Chrome** and **Edge**. Firefox and Safari are not supported.

1. Enter your ESP32-CAM's IP in the bar at the bottom → **▶ STREAM**
2. Click **▶ CONNECT BLE** in the sidebar → select your device name from the popup
3. Drive using keyboard or the on-screen D-pad

### Keyboard Controls

| Key | Action |
|-----|--------|
| `W` or `↑` | Forward |
| `S` or `Space` | Stop |
| `A` or `←` | Turn Left |
| `D` or `→` | Turn Right |
| `X` or `↓` | Backward |

Hold the key to keep moving. Release to stop.

### Features

| Feature | Detail |
|---------|--------|
| Live camera | MJPEG stream over WiFi — no plugin needed |
| Motor speed | Slider 80–255, or presets SLOW / MED / FULL |
| Humidity & temp | DHT11 readings pushed via BLE every 2 seconds |
| High humidity alert | Alert banner when humidity ≥ 80% |
| Video recording | Records to `.webm` file with humidity graph overlay |
| Auto-reconnect | BLE reconnects automatically up to 10 retries on drop |
| Watchdog | Motors auto-stop if no command for 300ms |

---

## BLE Protocol

Use these to build your own controller app or integrate with other systems.

| Command | Direction | Meaning |
|---------|-----------|---------|
| `F` | App → ESP32 | Move forward |
| `B` | App → ESP32 | Move backward |
| `L` | App → ESP32 | Turn left |
| `R` | App → ESP32 | Turn right |
| `S` | App → ESP32 | Stop |
| `V:200` | App → ESP32 | Set speed to 200 (0–255) |
| `H:65.2:28.5` | ESP32 → App | Humidity 65.2%, temperature 28.5°C |

```
Service UUID       : 0000ffe0-0000-1000-8000-00805f9b34fb
Characteristic UUID: 0000ffe1-0000-1000-8000-00805f9b34fb
Device Name        : ESP32_Robot_KMUTT  (configurable)
Properties         : WRITE, WRITE_NO_RESPONSE, NOTIFY
```

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| BLE device not visible in picker | Wrong browser | Chrome or Edge only |
| BLE connects then immediately drops | Weak signal | ESP32 already uses max power (P9). Check 9V battery level. |
| Motors don't move | BLE not connected, or wrong pins | Serial Monitor should show `[BLE] Client connected` and `[CMD] F` etc. |
| Robot goes backward on Forward | M1B/M2A polarity reversed | Swap the motor wire at the Maker Drive terminal (M1A↔M1B or M2A↔M2B) |
| One side doesn't move | Loose wire or wrong GPIO | Check `[PWM]` output in Serial Monitor, check wiring to Maker Drive |
| DHT11 always reads NaN | Missing pull-up resistor or cold start | Add 10KΩ between DATA and 3.3V; the code waits 2s on boot but some sensors need longer |
| LDR LED never responds | Threshold value wrong for your LDR | Open Serial Monitor — `[LDR]` lines show the ADC reading. Adjust `THRESHOLD` to be just above your ambient light reading |
| Camera shows "No Signal" | IP mismatch or different WiFi | Confirm camera and browser device are on the same WiFi network |
| Camera stream is very slow / choppy | Bandwidth too high | Switch to `FRAMESIZE_QQVGA` in camera firmware |
| Recording fails | CORS headers missing | The camera sketch includes `Access-Control-Allow-Origin: *` — ensure it wasn't removed |
| Motors keep stopping randomly | Watchdog firing due to BLE delay | Keep browser tab active and in foreground; BLE must send every 100ms |

---

## License

MIT — free to use, modify, and distribute. Attribution appreciated but not required.

*Built at KMUTT — King Mongkut's University of Technology Thonburi*
