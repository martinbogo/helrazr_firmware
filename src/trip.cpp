/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "trip.h"
#include <math.h>

static double   s_dist     = 0.0; // metres
static uint32_t s_movingS  = 0;
static float    s_maxKmh   = 0.0f;
static float    s_lastLat  = 0.0f, s_lastLon = 0.0f;
static bool     s_have     = false;

static const float MOVING_KMH = 1.5f; // below this we don't count time/distance

static float hav_m(float lat1, float lon1, float lat2, float lon2) {
    const float R = 6371000.0f;
    float dlat = radians(lat2 - lat1), dlon = radians(lon2 - lon1);
    float a = sinf(dlat / 2) * sinf(dlat / 2) +
              cosf(radians(lat1)) * cosf(radians(lat2)) * sinf(dlon / 2) * sinf(dlon / 2);
    return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

void trip_reset() {
    s_dist = 0; s_movingS = 0; s_maxKmh = 0; s_have = false;
}

// Call at ~1 Hz. Only accumulates while actually moving, and rejects GPS noise
// (sub-2 m jitter) and implausible jumps (>100 m/s) so the odometer stays sane.
void trip_update(bool fix, float lat, float lon, float spd_kmh) {
    if (!fix) return;
    if (spd_kmh > s_maxKmh) s_maxKmh = spd_kmh;
    bool moving = spd_kmh >= MOVING_KMH;
    if (moving) s_movingS++;
    if (s_have && moving) {
        float d = hav_m(s_lastLat, s_lastLon, lat, lon);
        if (d > 2.0f && d < 100.0f) s_dist += d;
    }
    s_lastLat = lat; s_lastLon = lon; s_have = true;
}

float    trip_distance_m() { return (float)s_dist; }
uint32_t trip_moving_s()   { return s_movingS; }
float    trip_max_kmh()    { return s_maxKmh; }
float    trip_avg_kmh()    {
    return s_movingS > 0 ? (float)(s_dist / 1000.0) / (s_movingS / 3600.0) : 0.0f;
}
