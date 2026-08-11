/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

// -----------------------------------------------------------------------------
// region.h - LoRa regulatory region (runtime-selectable)
//
// Meshtastic operates in different ISM sub-bands depending on where you are.
// The selected region re-targets everything that touches RF: the channel
// presets monitored in Monitor/Duty/FreqOffset, the default receive frequency,
// and the sweep ranges used by Spectrum/Waterfall/AutoTrack.
//
// The region can be changed on the device itself from the Settings menu; the
// choice is persisted and restored on boot (see region.cpp), exactly like the
// theme setting. LORA_REGION (below) only sets the *initial* default used the
// first time the firmware runs, before anything has been saved. Set it in
// src/config.h or via a build flag, e.g.:
//
//     #define LORA_REGION RG_ANZ          // in src/config.h
//     pio run -e t114 -D LORA_REGION=RG_ANZ
//
// Band edges are taken verbatim from the Meshtastic firmware region table
// (src/mesh/RadioInterface.cpp). Per-preset centre frequencies are computed
// from those edges using Meshtastic's own frequency-slot algorithm (djb2 hash
// of the channel name, modulo the number of slots in the band), so a preset
// lands on exactly the frequency a real Meshtastic node would use.
// -----------------------------------------------------------------------------

#pragma once
#include <Arduino.h>
#include "config.h"   // user may set LORA_REGION here; must be seen first

// --- Region codes -----------------------------------------------------------
// Values are stable identifiers persisted to flash; do not renumber existing
// entries. Use these with #define LORA_REGION and in region_set_by_code().
#define RG_US       1    // 902.0 - 928.0 MHz  (United States, 915 MHz ISM)
#define RG_EU_868   2    // 869.4 - 869.65 MHz (Europe, 868 MHz, 10% duty cycle)
#define RG_EU_433   3    // 433.0 - 434.0 MHz  (Europe, 433 MHz)
#define RG_CN       4    // 470.0 - 510.0 MHz  (China)
#define RG_JP       5    // 920.5 - 923.5 MHz  (Japan)
#define RG_ANZ      6    // 915.0 - 928.0 MHz  (Australia / New Zealand)
#define RG_ANZ_433  7    // 433.05 - 434.79 MHz (Australia / New Zealand, 433)
#define RG_RU       8    // 868.7 - 869.2 MHz  (Russia)
#define RG_KR       9    // 920.0 - 923.0 MHz  (South Korea)
#define RG_TW       10   // 920.0 - 925.0 MHz  (Taiwan)
#define RG_IN       11   // 865.0 - 867.0 MHz  (India)
#define RG_NZ_865   12   // 864.0 - 868.0 MHz  (New Zealand 865)
#define RG_TH       13   // 920.0 - 925.0 MHz  (Thailand)
#define RG_UA_433   14   // 433.0 - 434.7 MHz  (Ukraine 433)
#define RG_UA_868   15   // 868.0 - 868.6 MHz  (Ukraine 868)
#define RG_MY_433   16   // 433.0 - 435.0 MHz  (Malaysia 433)
#define RG_MY_919   17   // 919.0 - 924.0 MHz  (Malaysia 919)
#define RG_SG_923   18   // 917.0 - 925.0 MHz  (Singapore 923)
#define RG_PH_433   19   // 433.0 - 434.7 MHz  (Philippines 433)
#define RG_PH_868   20   // 868.0 - 869.4 MHz  (Philippines 868)
#define RG_PH_915   21   // 915.0 - 918.0 MHz  (Philippines 915)
#define RG_BR_902   22   // 902.0 - 907.5 MHz  (Brazil 902)
#define RG_NP_865   23   // 865.0 - 868.0 MHz  (Nepal 865)
#define RG_LORA_24  24   // 2400.0 - 2483.5 MHz (worldwide 2.4 GHz; SX128x only)

// --- Compile-time default ---------------------------------------------------
// Used only until a region has been chosen and saved on the device.
#ifndef LORA_REGION
#define LORA_REGION RG_US
#endif

// --- Runtime API (mirrors theme.*) ------------------------------------------
void        region_init();               // load persisted choice (or default)
int         region_count();              // number of regions in the table
const char* region_name(int idx);        // display name for a table index
int         region_current();            // current table index
int         region_current_code();       // current RG_* code
void        region_set(int idx);         // clamp, persist, retune the radio
void        region_next();               // cycle to the next selectable region

// True when a region is usable on this board's radio. The sub-GHz SX1262 fitted
// to every supported board cannot reach the 2.4 GHz band, so RG_LORA_24 is not
// selectable on-device (it remains a valid compile-time target for SX128x work).
bool        region_selectable(int idx);

// Current band edges, in MHz, for the sweep screens and status output.
float       region_freq_start_mhz();
float       region_freq_end_mhz();
float       region_span_mhz();

// Centre frequency (MHz) of a Meshtastic modem preset in the current region.
// `meshName` is the canonical Meshtastic channel name (e.g. "MediumSlow"), from
// which the frequency-slot hash is computed.
float       region_channel_freq(const char* meshName, int bwKHz);
