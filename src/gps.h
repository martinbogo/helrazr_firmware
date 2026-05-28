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

void gps_cmd_raw();
void gps_cmd_init();
void gps_cmd_monitor();
void gps_diagnostic_test();
