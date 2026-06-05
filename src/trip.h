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

// Trip computer: odometer, moving time, and average/max speed. Accumulated by
// calling trip_update() at ~1 Hz from the main loop, independent of the page.
void     trip_update(bool fix, float lat, float lon, float spd_kmh);
void     trip_reset();
float    trip_distance_m();
uint32_t trip_moving_s();
float    trip_avg_kmh();
float    trip_max_kmh();
