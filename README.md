# Helrazr Firmware

[![License: CC BY-NC-ND 4.0](https://licensebuttons.net/l/by-nc-nd/4.0/88x31.png)](https://creativecommons.org/licenses/by-nc-nd/4.0/)

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

### LoRa Region

The firmware defaults to the **US** band (902-928 MHz). To build for another
regulatory region (ANZ, EU, etc.), set `LORA_REGION` in [`src/config.h`](src/config.h)
before building:

```c
// src/config.h
#define LORA_REGION RG_ANZ
```

This single setting re-targets everything that touches RF: the Meshtastic
channel presets used by Monitor / Duty / FreqOffset, the default receive
frequency, and the Spectrum / Waterfall / AutoTrack sweep ranges. Per-preset
frequencies are computed with Meshtastic's own frequency-slot algorithm, so each
preset lands on exactly the frequency a real Meshtastic node would use in that
region.

Supported region codes (see [`src/region.h`](src/region.h) for band edges):

`RG_US` `RG_EU_868` `RG_EU_433` `RG_ANZ` `RG_ANZ_433` `RG_JP` `RG_KR` `RG_TW`
`RG_IN` `RG_RU` `RG_CN` `RG_TH` `RG_NZ_865` `RG_UA_433` `RG_UA_868` `RG_MY_433`
`RG_MY_919` `RG_SG_923` `RG_PH_433` `RG_PH_868` `RG_PH_915` `RG_BR_902`
`RG_NP_865` `RG_LORA_24`

You can also override it at build time without editing files, e.g.
`pio run -e t114 -D LORA_REGION=RG_ANZ`. The active region and band are shown in
the serial shell `status` and `lora` command output.

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
| **Spectrum** | Sweeps the configured region's band (53 points, e.g. 902.0–928.0 MHz for US) and draws a real-time RSSI bar graph mapping signal strength. Prints peak frequency info. <br>**Short Press:** Toggles *Peak Hold* mode (displays an `[H]` indicator and draws a continuous line holding the historically highest signal levels). |
| **Scanner** | Sweeps the band and lists the top most active frequencies with RSSI above a threshold. Useful for finding active channels. |
| **Monitor** | Cycles through the standard Meshtastic channel presets (LongFast, LongSlow, etc.) for the configured region, dwelling on each to count packets and display a live activity table. |
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
