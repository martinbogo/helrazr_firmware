/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

// -----------------------------------------------------------------------------
// region.h - LoRa regulatory region selection (single source of truth)
//
// Meshtastic operates in different ISM sub-bands depending on where you are.
// Selecting a region here re-targets everything that touches RF: the channel
// presets monitored in Monitor/Duty/FreqOffset, the default receive frequency,
// and the sweep ranges used by Spectrum/Waterfall/AutoTrack.
//
// To build for a region other than the US default, define LORA_REGION before
// this header is seen. The easiest place is src/config.h, e.g.:
//
//     #define LORA_REGION RG_ANZ
//
// or on the PlatformIO build_flags line, e.g. `-D LORA_REGION=RG_ANZ`.
//
// The band edges below are taken verbatim from the Meshtastic firmware region
// table (src/mesh/RadioInterface.cpp). The per-preset centre frequencies are
// computed from those edges using Meshtastic's own frequency-slot algorithm
// (djb2 hash of the channel name, modulo the number of slots in the band), so a
// preset lands on exactly the same frequency a real Meshtastic node would use.
// -----------------------------------------------------------------------------

#pragma once
#include <Arduino.h>
#include "config.h"   // user may set LORA_REGION here; must be seen first

// --- Region codes -----------------------------------------------------------
// Values are arbitrary but must be unique. Use these with #define LORA_REGION.
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
#define RG_LORA_24  24   // 2400.0 - 2483.5 MHz (worldwide 2.4 GHz)

// --- Default region ---------------------------------------------------------
// Preserves the previous behaviour (US 915 MHz ISM) when nothing is selected.
#ifndef LORA_REGION
#define LORA_REGION RG_US
#endif

// --- Band edges per region (kHz, integer to keep the slot math exact) -------
#if   LORA_REGION == RG_US
  #define REGION_NAME            "US"
  #define REGION_FREQ_START_KHZ  902000L
  #define REGION_FREQ_END_KHZ    928000L
#elif LORA_REGION == RG_EU_868
  #define REGION_NAME            "EU_868"
  #define REGION_FREQ_START_KHZ  869400L
  #define REGION_FREQ_END_KHZ    869650L
#elif LORA_REGION == RG_EU_433
  #define REGION_NAME            "EU_433"
  #define REGION_FREQ_START_KHZ  433000L
  #define REGION_FREQ_END_KHZ    434000L
#elif LORA_REGION == RG_CN
  #define REGION_NAME            "CN"
  #define REGION_FREQ_START_KHZ  470000L
  #define REGION_FREQ_END_KHZ    510000L
#elif LORA_REGION == RG_JP
  #define REGION_NAME            "JP"
  #define REGION_FREQ_START_KHZ  920500L
  #define REGION_FREQ_END_KHZ    923500L
#elif LORA_REGION == RG_ANZ
  #define REGION_NAME            "ANZ"
  #define REGION_FREQ_START_KHZ  915000L
  #define REGION_FREQ_END_KHZ    928000L
#elif LORA_REGION == RG_ANZ_433
  #define REGION_NAME            "ANZ_433"
  #define REGION_FREQ_START_KHZ  433050L
  #define REGION_FREQ_END_KHZ    434790L
#elif LORA_REGION == RG_RU
  #define REGION_NAME            "RU"
  #define REGION_FREQ_START_KHZ  868700L
  #define REGION_FREQ_END_KHZ    869200L
#elif LORA_REGION == RG_KR
  #define REGION_NAME            "KR"
  #define REGION_FREQ_START_KHZ  920000L
  #define REGION_FREQ_END_KHZ    923000L
#elif LORA_REGION == RG_TW
  #define REGION_NAME            "TW"
  #define REGION_FREQ_START_KHZ  920000L
  #define REGION_FREQ_END_KHZ    925000L
#elif LORA_REGION == RG_IN
  #define REGION_NAME            "IN"
  #define REGION_FREQ_START_KHZ  865000L
  #define REGION_FREQ_END_KHZ    867000L
#elif LORA_REGION == RG_NZ_865
  #define REGION_NAME            "NZ_865"
  #define REGION_FREQ_START_KHZ  864000L
  #define REGION_FREQ_END_KHZ    868000L
#elif LORA_REGION == RG_TH
  #define REGION_NAME            "TH"
  #define REGION_FREQ_START_KHZ  920000L
  #define REGION_FREQ_END_KHZ    925000L
#elif LORA_REGION == RG_UA_433
  #define REGION_NAME            "UA_433"
  #define REGION_FREQ_START_KHZ  433000L
  #define REGION_FREQ_END_KHZ    434700L
#elif LORA_REGION == RG_UA_868
  #define REGION_NAME            "UA_868"
  #define REGION_FREQ_START_KHZ  868000L
  #define REGION_FREQ_END_KHZ    868600L
#elif LORA_REGION == RG_MY_433
  #define REGION_NAME            "MY_433"
  #define REGION_FREQ_START_KHZ  433000L
  #define REGION_FREQ_END_KHZ    435000L
#elif LORA_REGION == RG_MY_919
  #define REGION_NAME            "MY_919"
  #define REGION_FREQ_START_KHZ  919000L
  #define REGION_FREQ_END_KHZ    924000L
#elif LORA_REGION == RG_SG_923
  #define REGION_NAME            "SG_923"
  #define REGION_FREQ_START_KHZ  917000L
  #define REGION_FREQ_END_KHZ    925000L
#elif LORA_REGION == RG_PH_433
  #define REGION_NAME            "PH_433"
  #define REGION_FREQ_START_KHZ  433000L
  #define REGION_FREQ_END_KHZ    434700L
#elif LORA_REGION == RG_PH_868
  #define REGION_NAME            "PH_868"
  #define REGION_FREQ_START_KHZ  868000L
  #define REGION_FREQ_END_KHZ    869400L
#elif LORA_REGION == RG_PH_915
  #define REGION_NAME            "PH_915"
  #define REGION_FREQ_START_KHZ  915000L
  #define REGION_FREQ_END_KHZ    918000L
#elif LORA_REGION == RG_BR_902
  #define REGION_NAME            "BR_902"
  #define REGION_FREQ_START_KHZ  902000L
  #define REGION_FREQ_END_KHZ    907500L
#elif LORA_REGION == RG_NP_865
  #define REGION_NAME            "NP_865"
  #define REGION_FREQ_START_KHZ  865000L
  #define REGION_FREQ_END_KHZ    868000L
#elif LORA_REGION == RG_LORA_24
  #define REGION_NAME            "LORA_24"
  #define REGION_FREQ_START_KHZ  2400000L
  #define REGION_FREQ_END_KHZ    2483500L
#else
  #error "Unknown LORA_REGION. See src/region.h for the list of RG_* codes."
#endif

// --- Derived band edges as floating-point MHz (for sweeps / display) --------
#define REGION_FREQ_START_MHZ  ((float)REGION_FREQ_START_KHZ / 1000.0f)
#define REGION_FREQ_END_MHZ    ((float)REGION_FREQ_END_KHZ   / 1000.0f)
#define REGION_SPAN_MHZ        (REGION_FREQ_END_MHZ - REGION_FREQ_START_MHZ)

// --- Meshtastic frequency-slot algorithm ------------------------------------
// All helpers are C++11-friendly constexpr (single-return, no loops) so the
// preset frequencies are resolved entirely at compile time.

// djb2 string hash (Dan Bernstein), matching Meshtastic RadioInterface.cpp.
constexpr uint32_t rf_djb2(const char* s, uint32_t h = 5381u) {
    return (*s == '\0') ? h
                        : rf_djb2(s + 1, ((h << 5) + h) + (uint32_t)(uint8_t)*s);
}

// Number of channel slots that fit in the band for a given bandwidth (kHz).
constexpr long rf_num_channels(long startKHz, long endKHz, long bwKHz) {
    return (endKHz - startKHz) / bwKHz;
}

// Zero-based channel slot for a channel name in the current band.
constexpr long rf_slot(const char* name, long startKHz, long endKHz, long bwKHz) {
    return (long)(rf_djb2(name) %
                  (uint32_t)rf_num_channels(startKHz, endKHz, bwKHz));
}

// Centre frequency (MHz) of the slot a channel name maps to. Mirrors
// Meshtastic: freq = freqStart + bw/2 + slot * bw.
constexpr float rf_channel_freq(const char* name,
                                long startKHz, long endKHz, long bwKHz) {
    return (float)(((double)startKHz * 1000.0
                    + (double)bwKHz * 500.0
                    + (double)rf_slot(name, startKHz, endKHz, bwKHz)
                          * (double)bwKHz * 1000.0)
                   / 1000000.0);
}

// Convenience: centre frequency of a preset in the currently selected region.
// `meshName` is the canonical Meshtastic channel name (e.g. "MediumSlow"),
// which is what the slot hash is computed from.
#define MESH_CHANNEL_FREQ(meshName, bwKHz) \
    rf_channel_freq((meshName), REGION_FREQ_START_KHZ, REGION_FREQ_END_KHZ, (bwKHz))
