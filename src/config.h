/*
 * user_config.h
 * 
 * Hardware and deployment overrides.
 */

#pragma once

// -------------------------------------------------------------
// USER CONFIGURATION
// -------------------------------------------------------------

// --- GPS Module Type Configuration ---
// Options:
// 1 = L76K GNSS (Standard / Default for Heltec V4 and T114)
// 2 = U-Blox M100 Mini (Custom UBX GPS)
#define GPS_MODULE_TYPE_L76K 1
#define GPS_MODULE_TYPE_M100 2

// Set your active GPS module here:
#define GPS_MODULE_TYPE GPS_MODULE_TYPE_L76K

// --- Custom GPS pin routing (uncomment and edit for non-default wiring) ---
// #define USE_CUSTOM_GPS_PINS
// #ifdef USE_CUSTOM_GPS_PINS
// #define CUSTOM_GPS_RX        8
// #define CUSTOM_GPS_TX        7
// #define CUSTOM_GPS_BAUD      115200
// #endif // USE_CUSTOM_GPS_PINS

// --- Example: T114 with external M100 u-blox ---
// #define GPS_MODULE_TYPE GPS_MODULE_TYPE_M100
// #define USE_CUSTOM_GPS_PINS
// #define CUSTOM_GPS_RX        8
// #define CUSTOM_GPS_TX        7
// #define CUSTOM_GPS_BAUD      115200

