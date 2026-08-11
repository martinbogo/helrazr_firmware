/*
 * user_config.h
 * 
 * Hardware and deployment overrides.
 */

#pragma once

// -------------------------------------------------------------
// USER CONFIGURATION
// -------------------------------------------------------------

// --- LoRa Regulatory Region ---
// Selects the ISM sub-band the firmware monitors. This re-targets the channel
// presets (Monitor / Duty / FreqOffset), the default receive frequency, and the
// Spectrum / Waterfall / AutoTrack sweep ranges to match the region you operate
// in. Frequencies are computed with Meshtastic's own slot algorithm, so presets
// land exactly where a real Meshtastic node would transmit.
//
// Uncomment ONE of the RG_* codes below (full list in src/region.h). Leaving it
// commented out keeps the previous default of US (902-928 MHz).
//
//   RG_US  RG_EU_868  RG_EU_433  RG_ANZ  RG_ANZ_433  RG_JP  RG_KR  RG_TW  RG_IN
//   RG_RU  RG_CN  RG_TH  RG_NZ_865  RG_UA_433  RG_UA_868  RG_MY_433  RG_MY_919
//   RG_SG_923  RG_PH_433  RG_PH_868  RG_PH_915  RG_BR_902  RG_NP_865  RG_LORA_24
//
// Example (Australia / New Zealand):
//   #define LORA_REGION RG_ANZ
//
// #define LORA_REGION RG_US

// --- GPS Module Type Configuration ---
// Options:
// 1 = L76K GNSS (Standard / Default for Heltec V4 and T114)
// 2 = U-Blox M100 Mini (Custom UBX GPS)
#define GPS_MODULE_TYPE_L76K 1
#define GPS_MODULE_TYPE_M100 2

// Set your active GPS module here:
#define GPS_MODULE_TYPE GPS_MODULE_TYPE_M100

// --- Custom GPS pin routing (uncomment and edit for non-default wiring) ---
// #define USE_CUSTOM_GPS_PINS
// #ifdef USE_CUSTOM_GPS_PINS
// #define CUSTOM_GPS_RX        8
// #define CUSTOM_GPS_TX        7
// #define CUSTOM_GPS_BAUD      115200
// #endif // USE_CUSTOM_GPS_PINS

// --- Example: T114 with external M100 u-blox ---
#define GPS_MODULE_TYPE GPS_MODULE_TYPE_M100
#define USE_CUSTOM_GPS_PINS
#define CUSTOM_GPS_RX        8
#define CUSTOM_GPS_TX        7
#define CUSTOM_GPS_BAUD      115200

