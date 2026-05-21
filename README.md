# Helrazr Firmware

Custom firmware for the Heltec Mesh Node T114 (nRF52840 + SX1262) and Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262). Provides LoRa spectrum analysis, Meshtastic packet decoding, node tracking, GPS tracking, and a USB serial control shell.

---

## Hardware Support

- **Heltec Mesh Node T114**: Nordic nRF52840 (ARM Cortex-M4F), Semtech SX1262, GPS (Quectel L76K), 1.14" TFT LCD.
- **Heltec WiFi LoRa 32 V3**: ESP32-S3, Semtech SX1262, 0.96" OLED display.

---

## Building and Flashing

### Prerequisites

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html) (`pip install platformio`)
- macOS/Linux with Python 3

### Build

Cross-platform helper scripts are provided that wrap `pio`. These scripts require Python 3 and will natively handle dependencies on Windows, macOS, and Linux:

```bash
# For Heltec T114
python build_and_flash.py -b t114

# For Heltec V3
python build_and_flash.py -b v3
```

### USB Flash

Use the `-p` parameter to automatically target a USB serial port:

#### Heltec T114
1. Double-tap the reset button quickly. The display shows the HT-n5262 bootloader screen.
2. Run:
```bash
python build_and_flash.py -b t114 -p /dev/tty.usbmodem101
```

#### Heltec V3
```bash
python build_and_flash.py -b v3 -p /dev/tty.usbmodem101
```

*(On Windows, use `COM3`. On Linux, use `/dev/ttyACM0`)*

### Bluetooth OTA Flash

Use the new OTA script tool to send firmware upgrades wirelessly:

```bash
python build_and_ota.py -b t114
# or
python build_and_ota.py -b v3
```

### Connect Serial Shell

```bash
screen /dev/tty.usbmodem101 115200
# or
pio device monitor -p /dev/tty.usbmodem101
```
Press `Ctrl-A K` to exit `screen`.

---

## Button UI

The single button navigates between modes.

| Press | In Menu | In a Mode |
|-------|---------|-----------|
| Short (<600 ms) | Advance to next mode | Varies by mode (e.g., toggle Peak Hold in Spectrum) |
| Long (≥600 ms) | Enter highlighted mode | Return to menu |

### Firmware Functions / Modes

| Mode | Description |
|------|-------------|
| **Status** | Shows a live dashboard with GPS position, LoRa RX stats, battery voltage, and firmware uptime. |
| **Spectrum** | Sweeps 902.0–928.0 MHz (53 points) and draws a real-time RSSI bar graph mapping signal strength. Prints peak frequency info. <br>**Short Press:** Toggles *Peak Hold* mode (displays an `[H]` indicator and draws a continuous line holding the historically highest signal levels). |
| **Scanner** | Sweeps the band and lists the top most active frequencies with RSSI above a threshold. Useful for finding active channels. |
| **Monitor** | Cycles through standard Meshtastic US915 channel presets (LongFast, LongSlow, etc.), dwelling on each to count packets and display a live activity table. |
| **Decoder** | Locks onto the Meshtastic LongFast channel to decode headers and parse text messages from unencrypted packets directly on the screen. |
| **Nodes** | Node Tracker mode. Maintains a localized database of heard Meshtastic nodes, tracking Node ID, packet counts, RSSI, and time since last seen. |
| **Stats** | Packet statistics mode. Shows total packets, packets per minute, and a visual histogram graph of recent traffic over time. Also tracks top nodes. |
| **AutoTrack** | Automatically scans the band for the strongest active frequency, locks onto it to listen for packets, and periodically rescans to maintain tracking. |
| **Standby** | Enters deep sleep / system off to preserve battery. |

---

## Serial Shell

Connect at 115200 baud. The prompt is `> `. Type `help` or `?` to list commands.

### Commands

#### System

| Command | Description |
|---------|-------------|
| `help` | List all available commands |
| `status` | Print full system status |
| `bat` | Print current battery voltage |
| `reboot` | Software reset |

#### GPS

| Command | Description |
|---------|-------------|
| `gps` | GPS fix status, satellite count, lat/lon/alt/speed (T114 mostly) |

#### LoRa

| Command | Description |
|---------|-------------|
| `lora` | Show current LoRa radio config and stats |
| `lora listen` | Start continuous receive mode |
| `lora stop` | Stop receiving (standby) |
| `lora freq <MHz>` | Set receive frequency |
| `lora bw <kHz>` | Set bandwidth |
| `lora sf <7-12>` | Set spreading factor |

#### Display & LED

| Command | Description |
|---------|-------------|
| `display on/off` | Power control for the display |
| `led on/off` | Turn the on-board green LED on or off |

---

## Standby / Power Off

- **Entering standby:** Select **Standby** from the menu and long-press to confirm, or hold the user button for **10 seconds** from anywhere.
- **Waking from standby (T114):** Press the button briefly. Do not hold the button when waking to prevent entering BLE OTA DFU mode.

---

## Known Limitations

- **Meshtastic decryption:** The decoder parses headers and attempts text decoding on the assumption the channel uses its default public key. Custom PSKs will show encrypted payloads.
- **Spectrum frequency accuracy:** The SX1262 TCXO provides ±1 ppm accuracy but the broad 0.5 MHz step size is mostly for finding active bands rather than fine-grained channel identification.
