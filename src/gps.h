/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#pragma once
#include <Arduino.h>

void gps_init();
void gps_update();

float gps_latitude();
float gps_longitude();
float gps_altitude();
float gps_speed_kmh();
int   gps_satellites();
bool  gps_has_fix();
uint32_t gps_chars_processed();
bool gps_is_m100_ok();

// Extended fields used by the GPS view pages.
float gps_course_deg();   // course over ground, degrees 0-360; -1 if unknown
bool  gps_course_valid(); // true if a recent course value is available
float gps_hdop();         // horizontal/position dilution of precision; 0 if unknown
bool  gps_present();      // runtime: a GPS module is attached and responding
bool  gps_time_valid();   // true if UTC date+time are valid
// Fills UTC date/time; any pointer may be null. Values are 0 when invalid.
void  gps_datetime(int *year, int *mon, int *day, int *hour, int *min, int *sec);

void gps_cmd_raw();
void gps_cmd_init();
void gps_cmd_monitor();
void gps_diagnostic_test();
