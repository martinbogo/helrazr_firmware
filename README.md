# Heltec T114 Custom Firmware

Custom firmware for the Heltec Mesh Node T114 (nRF52840 + SX1262). Provides LoRa spectrum analysis, Meshtastic packet decoding, GPS tracking, and a USB serial control shell.

---

## Hardware

| Component | Details |
|-----------|---------|
| MCU | Nordic nRF52840, ARM Cortex-M4F @ 64 MHz, 1 MB flash, 256 KB RAM |
| Radio | Semtech SX1262, 902–928 MHz (US ISM), up to +21 dBm TX |
| GPS | Quectel L76K, GPS/GLONASS/BeiDou/QZSS, UART @ 9600 baud |
| Display | 1.14" TFT LCD, ST7789, 240×135 px, landscape orientation |
| Flash | Macronix MX25R1635F, 2 MB QSPI |
| Button | Single user button (P1.10) |
| LED | Green LED (P1.03, active low) |
| NeoPixels | 2× SK6812 (P0.14) — not yet used |

---

## Building and Flashing

### Prerequisites

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html) (`pip install platformio`)
- macOS/Linux with Python 3

### Build

```bash
cd t114_exploratory
pio run
```

Output binary is at `.pio/build/t114/firmware.zip`.

### Flash

1. Double-tap the reset button quickly. The display shows the HT-n5262 bootloader screen (blue/green).
2. Run:

```bash
/path/to/.platformio/packages/framework-arduinoadafruitnrf52/tools/adafruit-nrfutil/macos/adafruit-nrfutil \
  dfu serial --package .pio/build/t114/firmware.zip \
  -p /dev/tty.usbmodem101 -b 115200
```

On Linux, replace `/dev/tty.usbmodem101` with the appropriate port (e.g. `/dev/ttyACM0`).

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
| Short (<600 ms) | Advance to next mode | No action |
| Long (≥600 ms) | Enter highlighted mode | Return to menu |

### Mode List

| Mode | Description |
|------|-------------|
| **Status** | GPS position, LoRa RX stats, battery, uptime |
| **Spectrum** | Real-time RSSI bar graph across 902–928 MHz |
| **Scanner** | Sweep band and list the most active frequencies |
| **Monitor** | Cycle through Meshtastic channel presets, count activity |
| **Decoder** | Listen on LongFast, parse Meshtastic packet headers and text messages |

---

## Modes

### Status

The default mode. Shows a two-column live dashboard:

```
  Heltec T114 Firmware
GPS              LoRa
Fix: 3D          Mode: RX
Sat: 9           RSSI: -94.0 dBm
Lat:  37.77490   SNR:   8.5 dB
Lon:-122.41940   Pkts: 3
-------------------------------
Bat: 3.85V       Up: 00:12:44
```

- **Fix** — `3D` when GPS has a valid position lock, `No` otherwise.
- **Sat** — number of satellites currently tracked.
- **Lat/Lon** — position in decimal degrees to 5 decimal places (~1 m precision).
- **Mode** — LoRa receiver state (`RX` = actively listening, `Idle` = standby).
- **RSSI** — signal strength of the last received LoRa packet (dBm).
- **SNR** — signal-to-noise ratio of the last received packet (dB).
- **Pkts** — total LoRa packets received since boot.
- **Bat** — estimated battery voltage. Color coded: green >3.5 V, yellow >3.2 V, red ≤3.2 V.
- **Up** — firmware uptime (HH:MM:SS).

The display refreshes every second.

---

### Spectrum

Sweeps 902.0–928.0 MHz in 0.5 MHz steps (53 points) and draws a live bar graph.

```
   Spectrum  902-928 MHz
   |         |    |
   |  |  ||  ||   |
 | || || || ||| | ||
902       910       920       928
Peak: 906.9MHz  -87dBm
```

- Bar height maps RSSI from –135 dBm (bottom) to –40 dBm (top).
- Bar color: **green** < –100 dBm (noise floor), **yellow** –100 to –80 dBm (weak signal), **red** ≥ –80 dBm (strong signal).
- Peak frequency and RSSI annotated below the graph.
- Full sweep takes ~265 ms; updates ~3–4 times per second.
- Each sweep also prints the peak to the serial console.

---

### Scanner

Same 902–928 MHz sweep as Spectrum, but output-focused. Lists the top 7 frequencies with RSSI above –100 dBm, sorted strongest first.

```
Channel Scanner
Threshold: -100 dBm

906.875  -87 dBm [==========       ]
915.000  -94 dBm [========         ]
912.500 -101 dBm [=====            ]

Found: 3 active
Scanning 902-928MHz...
```

- Re-sweeps every 2 seconds.
- Results also printed to serial in full.
- Useful for quickly finding which frequencies are in use before tuning in.

---

### Monitor

Cycles through all 7 standard Meshtastic US915 channel presets, dwelling 800 ms on each. Displays a live table of packet activity per channel.

```
Channel Monitor
Channel    Pkts  RSSI    Age
----------------------------------------
LongFast     12  -94dBm   2s
LongMod       0   --      --
LongSlow      1  -101dBm  45s
MedFast       0   --      --
MedSlow       0   --      --
ShortFast     3  -88dBm   12s
ShortSlow     0   --      --
Listening: LongFast
```

**Channel presets (US915):**

| Name | Frequency | BW | SF | CR |
|------|-----------|----|----|-----|
| LongFast | 906.875 MHz | 250 kHz | 11 | 4/8 |
| LongMod | 906.875 MHz | 125 kHz | 11 | 4/8 |
| LongSlow | 906.875 MHz | 125 kHz | 12 | 4/8 |
| MedFast | 906.875 MHz | 250 kHz | 9 | 4/8 |
| MedSlow | 906.875 MHz | 250 kHz | 10 | 4/8 |
| ShortFast | 906.875 MHz | 250 kHz | 7 | 4/5 |
| ShortSlow | 906.875 MHz | 250 kHz | 8 | 4/5 |

The currently active channel is highlighted in yellow. Each received packet is also logged to serial.

---

### Decoder

Locks onto the Meshtastic **LongFast** channel and parses the raw LoRa payload using the Meshtastic packet header format.

```
Meshtastic Decoder
From: 4A3C1B2D
To:   FFFFFFFF
Hop:2 RSSI:-94 SNR:7
MSG: Hello from node 4A3C

4A3C1B2D->FFFFFFFF  2s ago
E1F200AA->FFFFFFFF  18s ago
9B44C031->4A3C1B2D  41s ago

LongFast  Pkts: 7
```

**Meshtastic packet header structure (16 bytes):**

| Bytes | Field | Description |
|-------|-------|-------------|
| 0–3 | Destination | Destination node ID (little-endian uint32). `0xFFFFFFFF` = broadcast. |
| 4–7 | Source | Source node ID (little-endian uint32). |
| 8–11 | Packet ID | Unique packet identifier. |
| 12 | Flags | Bits 3–1: hop limit (0–7). Bit 6: want_ack. Bit 7: via_mqtt. |
| 13 | Channel hash | Hash of the channel name + PSK. |
| 14+ | Payload | Protobuf-encoded `Data` message (may be encrypted). |

**Text message decoding:** If the payload is unencrypted (as on the default public LongFast channel), the firmware attempts to decode it as a Meshtastic protobuf `Data` message. If field 2 contains printable UTF-8 text, it is shown on the display and printed to serial.

**Serial output per packet:**
```
--- Meshtastic Packet ---
  From:     0x4A3C1B2D
  To:       0xFFFFFFFF
  PktID:    0x0012AB34
  HopLim:   2
  ChanHash: 0x08
  RSSI:     -94.0 dBm
  SNR:      7.2 dB
  Text:     "Hello from node 4A3C"
  Raw[22]: 4A 3C 1B 2D FF FF FF FF 34 AB 12 00 04 08 0A 10 ...
```

---

## Serial Shell

Connect at 115200 baud. The prompt is `> `. Type `help` or `?` to list commands.

### Commands

#### System

| Command | Description |
|---------|-------------|
| `help` | List all available commands |
| `status` | Print full system status (GPS, LoRa, battery, uptime) |
| `bat` | Print current battery voltage |
| `reboot` | Software reset |

#### GPS

| Command | Description |
|---------|-------------|
| `gps` | GPS fix status, satellite count, lat/lon/alt/speed |

Example output:
```
Fix:   3D
Sats:  9
Lat:   37.774900
Lon:   -122.419400
Alt:   12.3 m
Speed: 0.0 km/h
Chars: 48320
```

`Chars` is the total number of NMEA bytes received from the GPS module — a non-zero value means the GPS UART is working even if there's no fix yet.

#### LoRa

| Command | Description |
|---------|-------------|
| `lora` | Show current LoRa radio config and stats |
| `lora listen` | Start continuous receive mode |
| `lora stop` | Stop receiving (standby) |
| `lora freq <MHz>` | Set receive frequency, e.g. `lora freq 906.875` |
| `lora bw <kHz>` | Set bandwidth: 7.8 / 10.4 / 15.6 / 20.8 / 31.25 / 41.7 / 62.5 / 125 / 250 / 500 |
| `lora sf <7-12>` | Set spreading factor (higher = longer range, slower) |

Example:
```
> lora
Mode: RX
Freq: 906.9 MHz
BW:   250 kHz
SF:   11
RSSI: -94.0 dBm
SNR:   7.2 dB
Pkts: 3
```

#### Display

| Command | Description |
|---------|-------------|
| `display on` | Power on display and backlight |
| `display off` | Power off display (saves ~11 mA) |

#### LED

| Command | Description |
|---------|-------------|
| `led on` | Turn green LED on |
| `led off` | Turn green LED off |

---

## Power Consumption

| State | Current |
|-------|---------|
| GPS off, display off | ~9 mA |
| GPS off, display on | ~20 mA |
| GPS on, display on | ~51 mA |
| Deep sleep (not yet implemented) | ~11 µA |

The battery is a LiPo connected to the SH1.25 JST connector. A solar panel input (5–6.5 V) is also supported on the second JST connector.

---

## Project Structure

```
t114_exploratory/
├── platformio.ini          Build configuration
├── boards/
│   └── heltec_mesh_node_t114.json   Custom board definition
├── variants/
│   └── heltec_mesh_node_t114/
│       ├── variant.h       Pin mapping and peripheral config
│       └── variant.cpp     GPIO pin map (1:1 Arduino→nRF52840)
└── src/
    ├── main.cpp            Setup, loop, mode dispatch, button handling
    ├── pins.h              All GPIO pin number definitions
    ├── modes.h             AppMode enum, Meshtastic channel preset table
    ├── button.h/.cpp       Debounced button driver, short/long press detection
    ├── menu.h/.cpp         Mode selection menu display
    ├── display.h/.cpp      ST7789 driver, status screen, drawing primitives
    ├── gps.h/.cpp          L76K GPS UART driver using TinyGPS++
    ├── lora.h/.cpp         SX1262 driver via RadioLib, scan/listen/decode
    ├── shell.h/.cpp        USB serial command shell
    ├── spectrum.h/.cpp     Spectrum analyzer mode
    ├── scanner.h/.cpp      Active channel scanner mode
    ├── monitor.h/.cpp      Multi-channel Meshtastic monitor mode
    └── decoder.h/.cpp      Meshtastic packet decoder mode
```

### Key Pin Assignments

| Signal | nRF52840 GPIO | Arduino Pin |
|--------|--------------|-------------|
| LoRa MOSI | P0.22 | 22 |
| LoRa MISO | P0.23 | 23 |
| LoRa SCK | P0.19 | 19 |
| LoRa CS | P0.24 | 24 |
| LoRa RST | P0.25 | 25 |
| LoRa BUSY | P0.17 | 17 |
| LoRa DIO1 | P0.20 | 20 |
| Display MOSI | P1.09 | 41 |
| Display SCK | P1.08 | 40 |
| Display CS | P0.11 | 11 |
| Display DC | P0.12 | 12 |
| Display RST | P0.02 | 2 |
| Display PWR | P0.03 | 3 |
| Display BL | P0.15 | 15 |
| GPS RX (CPU←GPS) | P1.05 | 37 |
| GPS TX (CPU→GPS) | P1.07 | 39 |
| GPS VEXT | P0.21 | 21 |
| Battery ADC | P0.04 | 4 |
| Battery ADC EN | P0.06 | 6 |
| Green LED | P1.03 | 35 |
| Button | P1.10 | 42 |

---

## Dependencies

Managed by PlatformIO (`platformio.ini`):

| Library | Purpose |
|---------|---------|
| `adafruit/Adafruit GFX Library` | Display graphics primitives |
| `adafruit/Adafruit ST7735 and ST7789 Library` | ST7789 TFT driver |
| `adafruit/Adafruit BusIO` | SPI/I2C abstraction |
| `mikalhart/TinyGPSPlus` | NMEA GPS sentence parser |
| `jgromes/RadioLib` | SX1262 LoRa radio driver |

Platform: `nordicnrf52` (Adafruit nRF52 BSP v1.7.0)

---

## Standby / Power Off (T114)

The T114 uses the nRF52840 System OFF mode for standby (~0.4 µA draw).

**Entering standby:**
- From the menu: select **Standby** and long-press to confirm. The display shows a shutdown screen for 2 seconds then the device enters System OFF.
- From anywhere: hold the button for **10 seconds**. At 5 seconds the NeoPixels turn amber and a countdown overlay appears on the display. At 10 seconds the device shuts down.

**Waking from standby:**
- Press the button briefly (press and release). The device resets and boots normally with a brief green NeoPixel flash.
- **Do not hold the button when waking.** The Adafruit/Heltec bootloader runs first after any reset. If it sees the button held it will enter BLE OTA DFU mode instead of booting the firmware.

**Implementation note — `GPREGRET = 0x6d`:**
Before entering System OFF, the firmware writes `0x6d` (DFU_MAGIC_SKIP) to the nRF52840 GPREGRET register via `sd_power_gpregret_set()`. The Adafruit bootloader reads this register on every reset; `0x6d` tells it to skip all DFU mode checks and boot the application immediately. Without this value the bootloader defaults to entering BLE OTA mode on wakeup. This behaviour is the same technique used by the Meshtastic firmware (`cpuDeepSleep()`).

---

## Known Limitations

- **Meshtastic decryption** — The default LongFast channel uses AES-128-CTR encryption. The decoder parses headers and attempts text decoding on the assumption the channel uses its default public key; nodes using custom PSKs will show encrypted payloads.
- **GPS cold start** — The L76K can take 30–90 seconds for a cold fix outdoors. `Chars > 0` in the `gps` command confirms the UART is working while waiting.
- **Spectrum frequency accuracy** — The SX1262 TCXO provides ±1 ppm accuracy. The 0.5 MHz step size is adequate for finding active bands but not fine-grained channel identification.
- **Button only** — No touchscreen. All control is via the single button or the USB serial shell.
