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
#define GPS_MODULE_TYPE GPS_MODULE_TYPE_M100

// Uncomment this if you are using an external/custom GPS on non-standard pins.
// By default, the firmware will use the standard pins for your board.
#define USE_CUSTOM_GPS_PINS

#ifdef USE_CUSTOM_GPS_PINS
// Define your custom GPS pins below. Example is for the M100 u-blox on T114.
// Note: On nRF52 (like T114), avoid using pins reserved for QSPI (e.g. 7 or 8 depending on hardware)
#define CUSTOM_GPS_RX        8
#define CUSTOM_GPS_TX        7
#define CUSTOM_GPS_BAUD      115200

// Uncomment and set these if your GPS module supports them:
// #define CUSTOM_GPS_VEXT      21
// #define CUSTOM_GPS_STANDBY   34
// #define CUSTOM_GPS_PPS       36
// #define CUSTOM_GPS_RST       38
#endif // USE_CUSTOM_GPS_PINS

