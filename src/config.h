/*
 * user_config.h
 * 
 * Hardware and deployment overrides.
 */

#pragma once

// -------------------------------------------------------------
// USER CONFIGURATION
// -------------------------------------------------------------

// Uncomment this if you are using an external/custom GPS on non-standard pins.
// By default, the firmware will use the standard pins for your board.
#define USE_CUSTOM_GPS_PINS

#ifdef USE_CUSTOM_GPS_PINS
// Define your custom GPS pins below. Example is for the M100 u-blox on T114.
// Note: On nRF52 (like T114), avoid using pins reserved for QSPI (e.g. 7 or 8 depending on hardware)
#define CUSTOM_GPS_RX        8
#define CUSTOM_GPS_TX        7

// Uncomment and set these if your GPS module supports them:
// #define CUSTOM_GPS_VEXT      21
// #define CUSTOM_GPS_STANDBY   34
// #define CUSTOM_GPS_PPS       36
// #define CUSTOM_GPS_RST       38
#endif // USE_CUSTOM_GPS_PINS

