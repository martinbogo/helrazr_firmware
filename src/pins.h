/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#pragma once

#if defined(HELTEC_T114)
// nRF52840 Arduino pin mapping: P0.xx = xx, P1.xx = 32 + xx

// --- LoRa SX1262 (SPIM2) ---
#define PIN_LORA_MOSI     22  // P0.22
#define PIN_LORA_MISO     23  // P0.23
#define PIN_LORA_SCK      19  // P0.19
#define PIN_LORA_CS       24  // P0.24
#define PIN_LORA_RST      25  // P0.25
#define PIN_LORA_BUSY     17  // P0.17
#define PIN_LORA_DIO1     20  // P0.20

// --- Display ST7789 (SPIM3) ---
#define PIN_TFT_MOSI      41  // P1.09
#define PIN_TFT_SCK       40  // P1.08
#define PIN_TFT_CS        11  // P0.11
#define PIN_TFT_DC        12  // P0.12
#define PIN_TFT_RST        2  // P0.02
#define PIN_TFT_PWR        3  // P0.03  (display power enable)
#define PIN_TFT_BL        15  // P0.15  (backlight, active LOW)

// --- GPS M100 Mini (UARTE1) ---
#define PIN_GPS_RX        8   // P0.08  (GPS TX -> CPU RX)
#define PIN_GPS_TX        7   // P0.07  (CPU TX -> GPS RX)
// #define PIN_GPS_STANDBY   34  // Not connected
// #define PIN_GPS_PPS       36  // Not connected
// #define PIN_GPS_VEXT      21  // Not connected
// #define PIN_GPS_RST       38  // Not connected

// --- Battery ADC ---
#define PIN_BAT_ADC        4  // P0.04  (AIN2)
#define PIN_BAT_ADC_EN     6  // P0.06  (HIGH enables voltage divider)
#define BAT_ADC_MULTIPLIER 4.916f

// --- LED & Button ---
#define PIN_LED           35  // P1.03  (active LOW)
#ifndef LED_STATE_ON
#define LED_STATE_ON      LOW
#endif
#ifndef LED_STATE_OFF
#define LED_STATE_OFF     HIGH
#endif
#define PIN_BUTTON        42  // P1.10

#elif defined(HELTEC_V3)
// Heltec WiFi LoRa 32 V3 (ESP32-S3)
// --- LoRa SX1262 ---
#define PIN_LORA_MOSI     10
#define PIN_LORA_MISO     11
#define PIN_LORA_SCK      9
#define PIN_LORA_CS       8
#define PIN_LORA_RST      12
#define PIN_LORA_BUSY     13
#define PIN_LORA_DIO1     14

// --- Display SSD1306 ---
#define PIN_OLED_SDA      17
#define PIN_OLED_SCL      18
#define PIN_OLED_RST      21   // typically 21 on v3, reset via RST pin

// --- Battery ADC ---
#define PIN_BAT_ADC       1
#define PIN_BAT_ADC_EN    37
#define BAT_ADC_MULTIPLIER 4.916f // Adjust as needed for v3

// --- LED & Button ---
#define PIN_LED           35  // onboard white LED
#define LED_STATE_ON      HIGH
#define LED_STATE_OFF     LOW
#define PIN_BUTTON        0   // PRG button

// --- No GPS ---
#undef PIN_GPS_RX
#undef PIN_GPS_TX

#elif defined(HELTEC_V4)
// Heltec WiFi LoRa 32 V4 (ESP32-S3R2)
// --- LoRa SX1262 ---
#define PIN_LORA_MOSI     10
#define PIN_LORA_MISO     11
#define PIN_LORA_SCK      9
#define PIN_LORA_CS       8
#define PIN_LORA_RST      12
#define PIN_LORA_BUSY     13
#define PIN_LORA_DIO1     14

// --- Display SSD1306 ---
#define PIN_OLED_SDA      17
#define PIN_OLED_SCL      18
#define PIN_OLED_RST      21

// --- Battery ADC ---
#define PIN_BAT_ADC       1
#define PIN_BAT_ADC_EN    37
#define BAT_ADC_MULTIPLIER 4.916f

// --- LED & Button ---
#define PIN_LED           35
#define LED_STATE_ON      HIGH
#define LED_STATE_OFF     LOW
#define PIN_BUTTON        0

// --- GPS GNSS ---
#define PIN_GPS_RX        39 // ESP32 RX (from GPS TX)
#define PIN_GPS_TX        38 // ESP32 TX (to GPS RX)
#define PIN_GPS_VEXT      34 // VGNSS Ctrl
#define PIN_GPS_STANDBY   40 // GNSS_Wake
#define PIN_GPS_PPS       41 
#define PIN_GPS_RST       42

// esp32-s3-devkitc-1 defines pins for a neopixel we don't have
#undef PIN_NEOPIXEL

#endif
