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

// Persisted user preferences (units, coordinate format, compass mode, screen
// brightness). Loaded once at startup; setters persist immediately.

enum Units    { UNITS_METRIC = 0, UNITS_IMPERIAL, UNITS_COUNT };
enum CoordFmt { COORD_DEG = 0, COORD_GRID, COORD_DDM, COORD_COUNT };

void prefs_init();

int  prefs_units();              void prefs_set_units(int);
int  prefs_coords();             void prefs_set_coords(int);
int  prefs_compass_mode();       void prefs_set_compass_mode(int);
int  prefs_brightness();         void prefs_set_brightness(int); // 0-100 (TFT)

const char* units_name(int u);
const char* coords_name(int c);

// Unit-aware formatters (respect prefs_units()/prefs_coords()).
void fmt_speed(float kmh, char *buf, size_t n);
void fmt_alt(float meters, char *buf, size_t n);
void fmt_dist(float meters, char *buf, size_t n);
void fmt_position(float lat, float lon, char *buf, size_t n);
void maidenhead(float lat, float lon, char *buf, size_t n);
