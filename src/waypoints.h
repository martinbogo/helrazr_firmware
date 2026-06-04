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

#define WP_MAX        16   // stored, persisted waypoints
#define WP_NAME_LEN   12
#define TRACK_MAX     250  // recorded track points held in RAM

struct Waypoint {
    char  name[WP_NAME_LEN];
    float lat;
    float lon;
    float alt;
};

struct TrackPoint {
    float    lat;
    float    lon;
    float    alt;
    uint16_t year;
    uint8_t  mon, day, hr, min, sec;
};

// Lifecycle
void wp_init();      // mount backend + load persisted waypoints

// Waypoints (persisted)
int             wp_count();
const Waypoint* wp_get(int i);
int             wp_add(float lat, float lon, float alt); // auto-named; returns index or -1 if full
int             wp_add_named(float lat, float lon, float alt, const char *name);
bool            wp_delete(int i);
void            wp_save();                                // persist current set

// Management (stored order == route order)
bool wp_rename(int i, const char *name);
bool wp_edit(int i, float lat, float lon, float alt);
bool wp_move(int from, int to);     // reorder; shifts items between
bool wp_move_up(int i);
bool wp_move_down(int i);
int  wp_project(int i, float bearingDeg, float distM, const char *name); // new wp; returns index or -1
void wp_sort_nearest(float lat, float lon); // destructive reorder by distance
void wp_sort_name();                        // destructive reorder by name

// Navigation controller: feeds the Wayfinder page.
enum NavMode { NAV_NONE = 0, NAV_GOTO, NAV_ROUTE };
void            nav_goto(int wpIndex);       // single target
void            nav_route_start(int fromIndex); // route from index to end
void            nav_stop();
NavMode         nav_mode();
const Waypoint* nav_target();                // current target waypoint, or null
int             nav_route_leg();             // 1-based current leg (route mode)
int             nav_route_total();           // total legs (route mode)
bool            nav_arrived_final();         // reached the end of the goto/route
// Advance route legs when within arriveM of the current target.
void            nav_update(bool fix, float lat, float lon, float arriveM);

// Track log (RAM ring buffer)
void track_set_recording(bool on);
bool track_is_recording();
int  track_count();
void track_clear();
// Append a point if recording; throttled by the caller. `valid` gates insertion.
void track_tick(bool valid, float lat, float lon, float alt,
                uint16_t year, uint8_t mon, uint8_t day,
                uint8_t hr, uint8_t min, uint8_t sec);

// Export waypoints + track as GPX (or CSV) to a stream (serial shell).
void gps_export_gpx(Stream &out);
void gps_export_csv(Stream &out);
