<!-- markdownlint-disable MD024 -->

# IKEA OBEGRÄNSAD Hack/Mod

Turn your OBEGRÄNSAD LED Wall Lamp into a live drawing canvas

> **⚠ Disclaimer**: Use this code and instructions at your own risk! Improper use may damage the device.
> **Contribute**: Have suggestions or improvements? Feel free to submit a PR or open an issue. 😊

![ezgif-3-2019fca7a4](https://user-images.githubusercontent.com/15351728/200184222-a590575d-983d-4ab8-a322-c6bcf433d364.gif)

## Table of Contents

- [Features](#features)
- [Quick Start](#quick-start)
- [Hardware Setup](#hardware-setup)
  - [Opening the Lamp](#opening-the-lamp)
  - [Understanding the Panels](#understanding-the-panels)
  - [Pin Configuration](#pin-configuration)
  - [Alternate Button Wiring](#alternate-button-wiring)
- [Software Setup](#software-setup)
  - [ESP32 Setup with VS Code and PlatformIO](#esp32-setup-with-vs-code-and-platformio)
  - [WiFi Configuration](#wifi-configuration)
  - [Flashing over USB, Backup and Restore](#flashing-over-usb-backup-and-restore)
- [OTA Updates](#ota-updates)
  - [Configuration](#configuration)
  - [Upload Methods](#upload-methods)
  - [Visual Feedback](#visual-feedback)
  - [Rollback](#rollback)
- [Bluetooth Control](#bluetooth-control)
  - [Device Protocol](#device-protocol)
- [HTTP API Reference](#http-api-reference)
  - [Device Information](#device-information)
  - [Plugin Control](#plugin-control)
  - [Display Control](#display-control)
  - [Message Display](#message-display)
  - [Plugin Scheduler](#plugin-scheduler)
  - [Storage Management](#storage-management)
- [DDP (Display Data Protocol)](#ddp-display-data-protocol)
- [Home Assistant Integration](#home-assistant-integration)
- [Development](#development)
  - [Plugin Development](#plugin-development)
- [Troubleshooting](#troubleshooting)
  - [Serial Diagnostics](#serial-diagnostics)

## Features

<details>
  <summary>Click to expand feature list</summary>

**General Features:**

- Persist your drawing
- Rotate image
- Live Drawing
- OTA Update, over WiFi or Bluetooth, with automatic rollback of a broken image
- WiFi Control, or no WiFi at all: the display never waits for the network
- Bluetooth Control from a web page (Chrome/Edge), including time sync
- Web GUI with a Device page for settings, diagnostics and WiFi credentials
- Load an image
- Draw arbitrary frames over WebSocket, Bluetooth or HTTP
- Switch plugin by pressing the button
- Schedule Plugins to switch after "n" seconds
- Serial diagnostics line every 5 seconds

**Available Plugins**, in the order of their ids and of the button cycle. The ones
marked ⚡ light most of the panel at once and need a proper power supply (see
[Troubleshooting](#troubleshooting)):

1. Big Clock
2. Game of Life
3. Box Dude: a tiny prisoner in his box, animated procedurally. He wanders, kicks a
   ball, knocks on the walls, naps, scratches tally marks and eventually breaks out
4. Matrix Rain
5. Pong Clock
6. Breakout
7. Snake
8. Firework
9. Radar
10. Meteor Shower
11. Fireflies
12. Spiral
13. Comet
14. Rain
15. Bubbles
16. Stars
17. Sparkle Field
18. Ticking Clock
19. Clock
20. Weather (needs WiFi)
21. Scanlines
22. Circle
23. Draw
24. Animation (with Animation Creator in Web UI)
25. DDP (Display Data Protocol, needs WiFi)
26. ArtNet (needs WiFi)
27. Wave Bars ⚡
28. Lines ⚡
29. Blobs ⚡
30. Checkerboard ⚡
31. Wave ⚡

The clocks need the time: from NTP over WiFi, or from the Bluetooth page, which sets
the clock whenever it connects.

</details>

## Quick Start

https://github.com/user-attachments/assets/ddf91be1-2c95-4adc-b178-05b0781683cc

Control the lamp using the built-in web GUI. Find the device IP address via:

- Serial monitor output
- Router admin panel

---

## Hardware Setup

This software is designed for ESP32 Dev Boards but can work with other Arduino boards (WiFi, OTA, and web server features will need to be removed for non-ESP boards).

**Supported Boards:**

- ESP32 Dev Board (recommended)
- TTGO LoRa32 V2.1 (T3_V1.6.1)
- ESP8266, e.g. NodeMCUv2 or Wemos D1 mini (with limitations: per-pixel brightness only works when storage and global brightness are disabled)

<img src="https://user-images.githubusercontent.com/15351728/200148521-86d0f9e6-2c41-4707-b2d9-8aa24a0e440e.jpg" width="60%" />

### Opening the Lamp

IKEA uses rivets instead of regular screws. To open:

1. Insert a screwdriver between the rivets and back panel
2. Carefully pry open with a second object
3. Alternative: Drill out the rivets (cleaner but permanent)

### Understanding the Panels

<img src="https://user-images.githubusercontent.com/15351728/200183585-39c1668d-665b-4c12-bcbb-387aec1d3874.JPG" width="60%" />

Inside you'll find 4 identical plates with 64 LEDs each (in 4 fields). Focus on the lowest plate:

- 6 connectors at the bottom edge (connection points for your board)
- Original microcontroller at the top (must be removed)

<img src="https://user-images.githubusercontent.com/86414213/205998862-e9962695-1328-49ea-b546-be592cbad3c2.jpg" width="90%" />

### Pin Configuration

Connect the pins as shown below. Remember to configure them in `include/constants.h` according to your board.

|       LCD        | ESP32  | ESP32-C3 | TTGO LoRa32 | NodeMCUv2 | Wemos D1 mini | Lolin D32 (Pro) | Xiao ESP32S3 |
| :--------------: | :----: | :------: | :---------: | :-------: | :-----------: | :-------------: | :----------: |
|       GND        |  GND   |   GND    |     GND     |    GND    |      GND      |       GND       |     GND      |
|       VCC        |   5V   |    5V    |     5V      |    VIN    |       5V      |       USB       |     VUSB     |
| EN (PIN_ENABLE)  | GPIO26 |  GPIO3   |    IO22     | GPIO16 D0 |   GPIO16 D0   |     GPIO26      |  D4 (GPIO5)  |
|  IN (PIN_DATA)   | GPIO27 |  GPIO4   |    IO23     | GPIO13 D7 |   GPIO13 D7   |     GPIO27      |  D10 (MOSI)  |
| CLK (PIN_CLOCK)  | GPIO14 |  GPIO5   |    IO02     | GPIO14 D5 |   GPIO14 D5   |     GPIO14      |   D8 (SCK)   |
| CLA (PIN_LATCH)  | GPIO12 |  GPIO6   |    IO15     | GPIO0 D3  |    GPIO0 D3   |     GPIO12      |  D5 (GPIO6)  |
|  BUTTON one end  | GPIO16 |  GPIO7   |    IO21     | GPIO2 D4  |    GPIO2 D4   |     GPIO25      |  D3 (GPIO4)  |
| BUTTON other end |  GND   |   GND    |     GND     |    GND    |      GND      |       GND       |     GND      |

<img src="https://user-images.githubusercontent.com/86414213/205999001-6213fc4f-be2f-4305-a17a-44fdc9349069.jpg" width="60%" />

### Alternate Button Wiring

You can use the original button wiring without adding external connections. See [this issue](https://github.com/ph1p/ikea-led-obegraensad/issues/79) by [RBEGamer](https://github.com/RBEGamer) for details.

---

## Software Setup

### ESP32 Setup with VS Code and PlatformIO

1. **Prerequisites**

   - Install [Visual Studio Code](https://code.visualstudio.com/)
   - Install the PlatformIO IDE extension from VS Code Extensions Marketplace

2. **Clone the Project**

   ```bash
   git clone git@github.com:ph1p/ikea-led-obegraensad.git
   cd ikea-led-obegraensad
   code .
   ```

   PlatformIO will automatically load dependencies.

3. **Connect ESP32**

   - Connect your ESP32 via USB
   - Verify the COM port in the PlatformIO Devices tab

4. **Configure the Project**

   - Run `PlatformIO: Clean` (Recycle bin icon in bottom toolbar)
   - Edit `include/secrets.h` with your WiFi credentials (ESP8266 only; ESP32 can use WiFi Manager)
   - Configure variables in `include/constants.h`

5. **Build the Project**

   - Click the `PlatformIO Build` icon (bottom toolbar)
   - If libraries are missing, install them via the PlatformIO Libraries tab
   - Repeat `Clean` and `Build` until successful

6. **Upload to ESP32**
   - Click `PlatformIO Upload` (bottom toolbar)

### WiFi Configuration

**ESP32 (WiFi Manager - Recommended):**

This project uses [tzapu's WiFiManager](https://github.com/tzapu/WiFiManager). WiFi never
blocks the display: the lamp starts drawing right away and connects in the background.

1. Device tries the saved network for 30 s (`WIFI_CONNECT_TIMEOUT_MS`)
2. If that fails, or nothing is saved, it opens a setup network named `IKEA`
   (`WIFI_MANAGER_SSID`) for 180 s (`WIFI_PORTAL_TIMEOUT_S`), then tries the saved network
   again, and so on
3. Connect to `IKEA` on any device; the captive portal guides you through the configuration
4. Device reboots once and connects to your network

Once the web server is running, a lost connection is only retried; the setup network does
not reopen until the next boot. Credentials can also be set, or forgotten, from the Device
page (over WiFi or Bluetooth), and WiFi can be switched off entirely there ("WiFi enabled",
stored as `wifiEnabled` in the config); the lamp then runs on Bluetooth only. Changing that
switch restarts the lamp.

**ESP8266 (Manual Configuration):**

For ESP8266, WiFi Manager is not available. Set `WIFI_SSID` and `WIFI_PASSWORD` in `include/secrets.h`.

### Flashing over USB, Backup and Restore

Some ESP32 boards (CP2102 USB bridges in particular) corrupt data above 115200 baud, so
the commands below use that speed. Adjust the port to yours (`pio device list`).

```bash
# back up the whole 4 MB flash (about 6 minutes), then check the file against the chip
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/cu.usbserial-0001 --baud 115200 read_flash 0 0x400000 backups/flash-full-4MB.bin
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/cu.usbserial-0001 --baud 115200 verify_flash 0 backups/flash-full-4MB.bin

# flash a fresh build: bootloader, partition table and app in one image
pio run -e esp32dev
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/cu.usbserial-0001 --baud 115200 write_flash 0 .pio/build/esp32dev/firmware.factory.bin

# restore the backup
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/cu.usbserial-0001 --baud 115200 write_flash 0 backups/flash-full-4MB.bin
```

`backups/` is git-ignored: a flash dump contains the stored WiFi credentials. Writing the
factory image also clears the settings, so the lamp starts in the first mode. If a board
does not enter the bootloader on its own, hold its BOOT (IO0) button while the command
connects.

---

## OTA Updates

Over-The-Air (OTA) updates allow you to upload new firmware wirelessly without a USB connection. This is powered by [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA).

### Configuration

Before using OTA, configure the following:

1. **Set OTA Credentials** in `include/secrets.h`:

   ```cpp
   #define OTA_USERNAME "admin"
   #define OTA_PASSWORD "your-password"
   ```

2. **Configure Upload Settings** in `platformio.ini` (for the `esp32dev` environment):
   ```ini
   extra_scripts = upload.py
   upload_protocol = custom
   custom_upload_url = http://192.168.178.50  # Your device IP
   custom_username = admin
   custom_password = your-password
   ```

**Note:** Replace `192.168.178.50` with your device's actual IP address.

### Upload Methods

#### Method 1: Web Interface (Manual Upload)

1. Navigate to `http://your-device-ip/update` in your browser
2. Login with your configured credentials (default: `admin` / `ikea-led-wall`)
3. Select your firmware file (`.pio/build/esp32dev/firmware.bin`)
4. Click "Update" and wait for completion
5. Device will automatically reboot with new firmware

#### Method 2: PlatformIO (Automated Upload)

Upload directly from PlatformIO via the command line:

```bash
pio run -e esp32dev -t upload
```

Or use the PlatformIO Upload button in VS Code (bottom toolbar).

**Requirements:**

- Python packages: `requests_toolbelt` and `tqdm`
- Install if needed: `pip install requests_toolbelt tqdm`

#### Method 3: Bluetooth

The Device page of the [Bluetooth page](#bluetooth-control) uploads a `firmware.bin` over
Bluetooth, in 4 KB windows that the lamp acknowledges; lost chunks are resent. It takes a
minute or two.

### Visual Feedback

During OTA updates, the LED matrix provides visual feedback:

- **"U" letter displayed**: Update has started
- **Serial output**: Progress updates every second
- **"R" letter displayed**: Update completed (device will reboot)

Monitor the serial output for detailed progress:

```
OTA update started!
OTA Progress Current: 262144 bytes, Final: 1440655 bytes
OTA Progress Current: 524288 bytes, Final: 1440655 bytes
...
OTA update finished successfully!
```

**Troubleshooting:**

- Ensure device is connected to the same network
- Verify IP address in `platformio.ini` matches device IP
- Check credentials match in both `secrets.h` and `platformio.ini`
- For upload failures, try the web interface method first

### Rollback

On ESP32 a freshly updated image (over WiFi or Bluetooth) stays "pending" until it has
run for 30 seconds (`OTA_ROLLBACK_GRACE_MS`). If it crashes or is power-cycled before
that, the bootloader boots the previous image again. Images flashed over USB are not
affected.

---

## Bluetooth Control

ESP32 boards also advertise themselves over Bluetooth LE as `IKEA-LED`, so the lamp can be
controlled where it cannot join the WiFi (for example a corporate network). The display never
waits for WiFi: it starts drawing right away, tries the saved network in the background and
opens the `IKEA` setup access point without blocking. WiFi can be switched off entirely on the
Device page.

- **Web page:** hosted at <https://letalvoj.github.io/obegraensad-bt-client/>, together with the
  latest `esp32dev` firmware. Build it with `cd frontend && pnpm build:ble` (output in
  `frontend/dist-ble`); `scripts/publish-bt-client.sh <site checkout>` rebuilds page and
  firmware into the `letalvoj.github.io` repository. It needs Chrome or Edge (Web Bluetooth)
  and HTTPS or `localhost`.
- **Pairing:** the first connection asks for a code, enter `003762` (`BLE_PASSKEY` in
  `include/constants.h`). Every characteristic requires the authenticated pairing.
- **Clock:** the page sets the lamp's clock on every connect, so the clock plugins work without
  NTP. The timezone comes from the settings.
- **Firmware updates:** the Device page streams a `firmware.bin` over Bluetooth. A freshly
  updated image has to run for 30 seconds before it is accepted; if it crashes before that, the
  bootloader goes back to the previous firmware.

### Device Protocol

The websocket (`/ws`) and Bluetooth carry the same JSON commands, `{"event": "<name>", ...}`:

| Event | Fields | Effect |
| --- | --- | --- |
| `info` | | Sends the current state (`info` event) |
| `plugin` / `persist-plugin` | `plugin` | Switches the mode / makes it the default |
| `brightness`, `rotate` | `brightness`, `direction` | Display settings |
| `frame` | `data`: 256 brightness values | Draws a frame and pauses the active mode |
| `pixel` | `x`, `y`, `value` | Sets one pixel and pauses the active mode |
| `stream` | `enabled` | `false` hands the screen back to the active mode |
| `schedule`, `schedule-start`, `schedule-stop`, `schedule-clear` | `schedule` | Scheduler |
| `message`, `message-remove` | `text`, `repeat`, `id`, ... | Scrolling messages |
| `config-get`, `config-set`, `config-reset` | `config` | Settings (`config` event) |
| `device-info` | | Diagnostics (`device` event) |
| `time` | `epoch` | Sets the clock |
| `wifi-credentials`, `wifi-forget` | `ssid`, `password` | Station WiFi |
| `reboot`, `factory-reset` | | Maintenance |
| `ota-begin`, `ota-status`, `ota-abort` | `size`, optional `md5` | Firmware update over Bluetooth (`ota` events) |

Binary 256-byte websocket messages are drawn as frames too. Commands report back with a
`result` event (`command`, `ok`, `message`).

**Over Bluetooth** (`include/ble.h`, `frontend/src/transport/ble.ts`), the lamp exposes one
GATT service, `0b5e0001-1ea0-4d1a-9b7e-0bec0ffee000`, with three characteristics:

| UUID | Direction | Content |
| --- | --- | --- |
| `0b5e0002-…` RX | write | messages from the browser |
| `0b5e0003-…` TX | notify | messages from the lamp |
| `0b5e0004-…` OTA | write without response | firmware chunks: `[offset, u32 little endian][bytes]` |

RX and TX messages are split into chunks. Each chunk starts with a header byte: `0x01` for
JSON or `0x02` for a 256-byte frame, OR-ed with `0x80` on the last chunk of a message.
A firmware update starts with `ota-begin`; the lamp answers with an `ota` event carrying the
MTU and window size, the browser sends a window of chunks and waits for the next `ota`
event (`progress`, `resend` with the offset to continue from, `done` or `error`).

## HTTP API Reference

Base URL: `http://your-server/api`

### Drawing and Diagnostics

```bash
# show an arbitrary frame: 512 hex digits, one brightness byte per pixel
curl "http://your-server/api/frame?data=$(printf 'ff%.0s' {1..256})"
# give the screen back to the active mode
curl "http://your-server/api/stream?enabled=0"
# firmware build, uptime, memory, WiFi and Bluetooth state
curl http://your-server/api/device
```

### Device Information

**Get device info, current state, and available plugins**

```http
GET /api/info
```

**Example:**

```bash
curl http://your-server/api/info
```

**Response:**

```json
{
  "rows": 16,
  "cols": 16,
  "status": "active",
  "plugin": 3,
  "rotation": 90,
  "brightness": 255,
  "scheduleActive": true,
  "schedule": [
    { "pluginId": 2, "duration": 60 },
    { "pluginId": 4, "duration": 120 }
  ],
  "plugins": [
    { "id": 1, "name": "Plugin One" },
    { "id": 2, "name": "Plugin Two" }
  ]
}
```

---

### Plugin Control

**Set active plugin by ID**

```http
PATCH /api/plugin?id={plugin_id}
```

**Example:**

```bash
curl -X PATCH "http://your-server/api/plugin?id=7"
```

**Response:**

```json
{
  "status": "success",
  "message": "Plugin set successfully"
}
```

**Error Response:**

```json
{
  "error": true,
  "errormessage": "Could not set plugin to id 7"
}
```

---

### Display Control

#### Set Brightness

```http
PATCH /api/brightness?value={0-255}
```

**Example:**

```bash
curl -X PATCH "http://your-server/api/brightness?value=100"
```

**Response:**

```json
{
  "status": "success",
  "message": "Brightness set successfully"
}
```

#### Get Display Data

Retrieve current display data as a byte-array. Each byte represents pixel brightness (0-255). Global brightness is applied after these values.

```http
GET /api/data
```

**Example:**

```bash
curl http://your-server/api/data
```

**Response:**

```json
[255, 255, 255, 0, 128, 255, 255, 0, ...]
```

---

### Message Display

#### Display a Message

Display scrolling text and/or graphs on the LED matrix.

```http
GET /api/message
```

**Parameters:**

- `text` (optional): Text message to display
- `graph` (optional): Comma-separated integers (0-15) representing a graph
- `miny` (optional): Graph lower bound (default: 0)
- `maxy` (optional): Graph upper bound (default: 15)
- `repeat` (optional): Repetition count (default: 1, use `-1` for infinite)
- `id` (optional): Unique identifier for the message
- `delay` (optional): Scroll delay in ms (default: 50)

**Example:**

```bash
curl "http://your-server/api/message?text=Hello&graph=8,5,2,1,0,0,1,4,7,10,13,14,15,15,14,11&repeat=3&id=1&delay=60"
```

**Response:**

```json
{
  "status": "success",
  "message": "Message received"
}
```

#### Remove a Message

```http
GET /api/removemessage?id={message_id}
```

**Example:**

```bash
curl "http://your-server/api/removemessage?id=1"
```

---

### Plugin Scheduler

Automatically switch between plugins on a schedule.

#### Set Schedule

```http
POST /api/schedule
```

**Example:**

```bash
curl -X POST http://your-server/api/schedule -d 'schedule=[{"pluginId":10,"duration":2},{"pluginId":8,"duration":5}]'
```

#### Start Schedule

```http
GET /api/schedule/start
```

#### Stop Schedule

```http
GET /api/schedule/stop
```

#### Clear Schedule

```http
GET /api/schedule/clear
```

---

### Storage Management

#### Clear Storage

```http
GET /api/clearstorage
```

**Example:**

```bash
curl http://your-server/api/clearstorage
```

---

## DDP (Display Data Protocol)

DDP enables real-time LED matrix control via UDP packets. External applications can send pixel data directly over the network.

### Python project setup with Poetry

Please [install Poetry](https://python-poetry.org/docs/#installation) for an easy way to setup the Python environment.

To install all required dependencies and create a virtual environment run:

```bash
poetry install
```

VS Code should detect the venv automatically and prompt you to activate it. Once that's done you can use the `python` command as usual. Alternatively use the `poetry run` command to make sure the python from the virtual environment is executed, e.g.:

```bash
poetry run python ddp.py clear
```

### Quick Start

1. **Enable DDP Plugin**

   ```bash
   curl -X PATCH "http://your-server/api/plugin?id=17"
   ```

2. **Send Pixels**

   ```bash
   python3 ddp.py --ip 192.168.178.50 --fill 128
   ```

### Using ddp.py

The included Python script (`ddp.py`) simplifies DDP packet creation. It offers a rudimentary command line interface to control display content.

**Clear all pixels:**

```bash
python3 ddp.py clear --ip 192.168.178.50
```

**Fill display with brightness value:**

```bash
python3 ddp.py fill 128 --ip 192.168.178.50
```

**Set individual pixels (X, Y, brightness):**

```bash
python3 ddp.py pixels --ip 192.168.178.50 -p 0 0 255 -p 15 15 128
```

**Subcommands:**

- `clear`: Clear all pixels
- `fill BRIGHTNESS`: Fill with brightness (0-255)
- `pixels -p X Y BRIGHTNESS`: Set pixel (-p can be used multiple times)

**Options (for all subcommands):**

- `--ip`: Display IP address (default: 192.168.178.50)
- `--port`: UDP port (default: 4048)
- `-v, --verbose`: Write more information to output
- `-d, --debug`: Write even more information to output

**Legacy options (still available for compatibility):**

- `--clear`: Clear all pixels
- `--fill BRIGHTNESS`: Fill with brightness (0-255)
- `--pixel X Y BRIGHTNESS`: Set pixel (can be used multiple times)

**Coordinates:**

- Matrix: 16×16 pixels
- X: 0-15 (left to right)
- Y: 0-15 (top to bottom)
- Brightness: 0-255

### Protocol Specification

**Packet Structure:**

```text
[Header: 10 bytes][RGB Data: 768 bytes for 16×16]
```

**Header (10 bytes):**

- Byte 0: `0x41` (Version 1)
- Byte 1: `0x00` (Flags)
- Bytes 2-9: `0x00` (Reserved)

**RGB Data:**

- 3 bytes per pixel (R, G, B)
- Total: 16 × 16 × 3 = 768 bytes
- Order: Row-major (left to right, top to bottom)
- Brightness calculated as: `(R + G + B) / 3`

**Single Pixel Mode:**
Send only 3 RGB bytes (total: 13 bytes) to apply the same color to all pixels.

**Example (Python):**

```python
import socket

# Create DDP packet
header = bytearray([0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
pixels = bytearray([128, 128, 128] * 256)  # Fill with mid-brightness
packet = header + pixels

# Send packet
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(packet, ("192.168.178.50", 4048))
sock.close()
```

---

## Home Assistant Integration

### HACS Integration (Recommended)

Use [this Home Assistant Integration](https://github.com/HennieLP/ikea-led-obegraensad-Home-Assistant-Integration) for easy setup with seamless integration and instant updates via websockets.

### HTTP API Integration

Example automation to adjust brightness based on sun position:

**configuration.yaml:**

```yaml
rest_command:
  obegraensad_brightness_high:
    url: 'http://your-server/api/brightness/'
    method: PATCH
    content_type: 'application/x-www-form-urlencoded'
    payload: 'value=100'
  obegraensad_brightness_low:
    url: 'http://your-server/api/brightness/'
    method: PATCH
    content_type: 'application/x-www-form-urlencoded'
    payload: 'value=1'
```

**Automation (Settings → Automations → Edit in YAML):**

```yaml
alias: Obegraensad low brightness
description: ''
triggers:
  - trigger: sun
    event: sunset
    offset: 0
conditions: []
actions:
  - action: rest_command.obegraensad_brightness_low
    data: {}
mode: single
```

Create a second automation or condition to call `rest_command.obegraensad_brightness_high` at sunrise.

---

## Development

### Arduino/C++ Development

**Structure:**

- `src/` - Arduino code
- `platformio.ini` - Build configuration
  - See [OTA Updates](#ota-updates) section for wireless firmware upload configuration

### Frontend Development

**Structure:**

- `frontend/` - Web UI code built with solidjs
- `frontend/src/` - Source files
- `frontend/dist/` - Built files (generated)

**Prerequisites:**

This project uses [pnpm](https://pnpm.io/) as the package manager. pnpm is an alternative to npm.

**Install pnpm:**

```bash
# Using npm
npm install -g pnpm

# Using Homebrew (macOS)
brew install pnpm

# Using Corepack (Node.js 16.13+)
corepack enable
corepack prepare pnpm@latest --activate
```

**Setup:**

```bash
cd frontend
pnpm install
```

**Configuration:**

Create a `.env` file in the `frontend/` directory and set your device IP:

```bash
VITE_DEVICE_IP=192.168.178.50
```

This allows the development server to proxy API requests to your device during development.

**Commands:**

- `pnpm dev` - Start development server with hot reload (usually at `http://localhost:5173`)
- `pnpm build` - Build optimized production bundle and generate `webgui.cpp` for ESP32
- `pnpm build:ble` - Build the Web Bluetooth variant into `frontend/dist-ble/`
- `pnpm preview` - Preview production build locally
- `pnpm lint` - Run linter to check code quality
- `pnpm format` - Format code with Prettier

**Development Workflow:**

1. Start the development server: `pnpm dev`
2. Make changes to files in `frontend/src/`
3. Changes will hot-reload in your browser
4. Test your changes against a running device
5. When ready, run `pnpm build` to generate the embedded web UI
6. The build process creates `src/webgui.cpp` which is compiled into the firmware

**Transports:** the UI talks to the lamp through `frontend/src/transport/`: a WebSocket
in the embedded build, or `BleTransport` (Web Bluetooth, mimicking the WebSocket API) when
built with `VITE_TRANSPORT=ble`. Both carry the [device protocol](#device-protocol).

**Look:** the UI is themed like an IKEA catalogue. Colours and fonts are Tailwind theme
tokens in `frontend/src/index.css` (`brand`, `accent`, `ink`, `paper`, `wall`, ...) with
shared `btn`, `section-title`, `page-title`, `slab` and `price-tag` utilities; the drawn
catalogue pieces (masthead, room, assembly steps) live in
`frontend/src/components/catalogue.tsx`.

**Publishing the Bluetooth page:** `scripts/publish-bt-client.sh <letalvoj.github.io checkout>`
builds the page and the esp32dev firmware into `obegraensad-bt-client/` of that repository;
commit and push it there.

**Docker Build:**

If you prefer using Docker instead of installing Node.js and pnpm locally:

```bash
docker compose run node
```

### Code Quality

**Pre-commit Hooks:**

```bash
# Install pre-commit
pip install pre-commit

# Activate hooks
pre-commit install
```

---

### Plugin Development

#### **1. Create Plugin Files**

**plugins/MyPlugin.h:**

```cpp
#pragma once

#include "PluginManager.h"

class MyPlugin : public Plugin {
public:
    MyPlugin();
    ~MyPlugin() override;

    void setup() override;
    void loop() override;
    const char* getName() const override;

    void teardown() override; // optional
    void websocketHook(JsonDocument &request) override; // optional
};
```

**plugins/MyPlugin.cpp:**

```cpp
#include "plugins/MyPlugin.h"

MyPlugin::MyPlugin() {
    // Constructor logic, if needed
}

void MyPlugin::setup() {
    // Setup logic for your plugin
}

void MyPlugin::loop() {
    // Loop logic for your plugin
}

const char* MyPlugin::getName() const {
    return "MyPlugin"; // name in GUI
}

void MyPlugin::teardown() {
    // code if plugin gets deactivated
}

void MyPlugin::websocketHook(JsonDocument &request) {
    // handle websocket requests
}
```

#### **2. Register Plugin in main.cpp**

```cpp
#include "plugins/MyPlugin.h"

pluginManager.addPlugin(new MyPlugin());
```

The registration order in `baseSetup()` gives the plugin ids and the order of the button
cycle, so inserting a plugin renumbers the ones after it (saved defaults and schedules
refer to ids). The web UI matches plugins by name, never by id.

**Keep `loop()` short and non-blocking:** it runs on the screen task. For the time, use
`getLocalTime(&timeinfo, 0)`: without the 0 it waits up to 5 seconds whenever the clock
has not been set yet, which is always the case without WiFi until the Bluetooth page syncs it.

---

## Troubleshooting

### Flickering Panel

- Verify all soldering points, especially VCC
- Ensure adequate power supply to the board
- Check for loose connections
- Try to connect the power supply directly to the microcontroller board

### Microcontroller is stuck in boot loop / flickering panel / black panel

- Break the 5V connection between the matrix display and the microcontroller board (cut cable between controller and VCC and insulate ends with tape)
- Use separate USB power cables for the microcontroller and the matrix display (e.g. by using a power supply with 2x5V output of 10W each)
- Reason: When voltage drops too low on the microcontroller board output pins might not reach 3.3V, since the matrix display is actually expecting a 0V-5V signal the resulting output voltage of the microcontroller might be lower than the threshold of the matrix display resulting in flickering / completely black matrix displays. Additionally must power supplies only deliver 10W per port which is not always sufficient to power microcontroller and matrix displays (especially at brightness 100% and more than half the pixels being lit)
- Has the additional advantage that you can upload firmware without dissassembly of the backpanel if you cannot use OTA updates

### Crashes or resets in bright modes

The same power problem, typically when the lamp runs from a laptop's USB port: modes that
light most of the panel at once (Game of Life, and the ⚡ modes at the end of the list) pull
the supply down until the board resets. Use a proper 5V supply, or stay with the sparse
modes.

### Black panel right after flashing

Not necessarily a fault: a fresh flash starts in mode 1 (Big Clock), which stays empty
until the clock is set by NTP or by connecting the Bluetooth page, and Draw shows your
saved drawing, which is empty on a fresh lamp. Press the button to go to the next mode, or
check the [serial diagnostics](#serial-diagnostics).

### Serial Diagnostics

Every 5 seconds (`DIAG_LOG_INTERVAL_MS`) the firmware prints one status line at 115200
baud, plus a line for every button press and mode switch:

```
[Diag] up 15s | mode 1 Big Clock | status 0 | brightness 255 | screen 70132 frames (+25025) | button pin 1 | loops 2636 | heap 67924 | wifi portal | ble idle
[Button] pattern 0, status 0
[Plugins] activating 2 GameOfLife
```

- `screen ... (+n)`: panel refreshes in the last 5 seconds, about 25000 when the screen
  timer runs; 0 means the display is not being driven
- `loops`: main loop iterations so far; it should grow by about 1000 every 5 seconds. A
  crawl means something blocks the loop, and the button stops responding
- `button pin`: raw level of the button pin, 1 released, 0 pressed
- `status`: 0 normal, 1 streaming frames, 2 firmware update, 3 loading
