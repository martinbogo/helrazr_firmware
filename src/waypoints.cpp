/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "waypoints.h"
#include "lora.h"
#include <math.h>
#include <string.h>

// ---- storage backends -------------------------------------------------------
//
// Waypoints persist as a small binary blob: a 1-byte count followed by the
// Waypoint array. ESP32 uses NVS via Preferences; nRF52840 uses the Adafruit
// LittleFS InternalFS. Other targets compile to a RAM-only no-op.

#if defined(ESP32)
#include <Preferences.h>
static Preferences prefs;
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;
static const char *WP_PATH = "/waypoints.dat";
#endif

static Waypoint  s_wp[WP_MAX];
static int       s_wp_count = 0;

static TrackPoint s_track[TRACK_MAX];
static int        s_track_count = 0;
static bool       s_track_head_wrapped = false; // ring buffer wrapped past start
static int        s_track_head = 0;             // next write index
static bool       s_recording = false;

// blob layout: [uint8 count][Waypoint * count]
static size_t blob_build(uint8_t *buf, size_t cap) {
    size_t need = 1 + (size_t)s_wp_count * sizeof(Waypoint);
    if (need > cap) return 0;
    buf[0] = (uint8_t)s_wp_count;
    memcpy(buf + 1, s_wp, (size_t)s_wp_count * sizeof(Waypoint));
    return need;
}

static void blob_load(const uint8_t *buf, size_t len) {
    if (len < 1) return;
    int n = buf[0];
    if (n < 0 || n > WP_MAX) return;
    if (len < 1 + (size_t)n * sizeof(Waypoint)) return;
    memcpy(s_wp, buf + 1, (size_t)n * sizeof(Waypoint));
    s_wp_count = n;
}

void wp_init() {
    s_wp_count = 0;
    static uint8_t buf[1 + WP_MAX * sizeof(Waypoint)];
#if defined(ESP32)
    prefs.begin("gpswp", true); // read-only
    size_t len = prefs.getBytes("wp", buf, sizeof(buf));
    prefs.end();
    if (len > 0) blob_load(buf, len);
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
    InternalFS.begin();
    File f(InternalFS);
    if (f.open(WP_PATH, FILE_O_READ)) {
        size_t len = f.read(buf, sizeof(buf));
        f.close();
        blob_load(buf, len);
    }
#endif
}

void wp_save() {
    static uint8_t buf[1 + WP_MAX * sizeof(Waypoint)];
    size_t len = blob_build(buf, sizeof(buf));
    if (len == 0) return;
#if defined(ESP32)
    prefs.begin("gpswp", false); // read-write
    prefs.putBytes("wp", buf, len);
    prefs.end();
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
    InternalFS.remove(WP_PATH);
    File f(InternalFS);
    if (f.open(WP_PATH, FILE_O_WRITE)) {
        f.write(buf, len);
        f.close();
    }
#else
    (void)buf; (void)len;
#endif
}

// ---- waypoints --------------------------------------------------------------

int wp_count() { return s_wp_count; }

const Waypoint* wp_get(int i) {
    if (i < 0 || i >= s_wp_count) return nullptr;
    return &s_wp[i];
}

int wp_add_named(float lat, float lon, float alt, const char *name) {
    if (s_wp_count >= WP_MAX) return -1;
    Waypoint &w = s_wp[s_wp_count];
    if (name && name[0]) snprintf(w.name, sizeof(w.name), "%s", name);
    else                 snprintf(w.name, sizeof(w.name), "WPT%02d", s_wp_count + 1);
    w.lat = lat; w.lon = lon; w.alt = alt;
    w.rssi = (int16_t)lroundf(lora_last_rssi()); // geo-tag the last RX signal
    w.snr  = (int8_t)lroundf(lora_last_snr());
    int idx = s_wp_count++;
    wp_save();
    return idx;
}

int wp_add(float lat, float lon, float alt) {
    return wp_add_named(lat, lon, alt, nullptr);
}

bool wp_delete(int i) {
    if (i < 0 || i >= s_wp_count) return false;
    for (int j = i; j < s_wp_count - 1; j++) s_wp[j] = s_wp[j + 1];
    s_wp_count--;
    nav_stop(); // indices shifted; drop any active navigation
    wp_save();
    return true;
}

// Great-circle distance in metres (used for sorting and projection).
static float wp_dist(float lat1, float lon1, float lat2, float lon2) {
    const float R = 6371000.0f;
    float dlat = radians(lat2 - lat1), dlon = radians(lon2 - lon1);
    float a = sinf(dlat / 2) * sinf(dlat / 2) +
              cosf(radians(lat1)) * cosf(radians(lat2)) * sinf(dlon / 2) * sinf(dlon / 2);
    return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

bool wp_rename(int i, const char *name) {
    if (i < 0 || i >= s_wp_count || !name) return false;
    snprintf(s_wp[i].name, sizeof(s_wp[i].name), "%s", name);
    wp_save();
    return true;
}

bool wp_edit(int i, float lat, float lon, float alt) {
    if (i < 0 || i >= s_wp_count) return false;
    s_wp[i].lat = lat; s_wp[i].lon = lon; s_wp[i].alt = alt;
    wp_save();
    return true;
}

bool wp_move(int from, int to) {
    if (from < 0 || from >= s_wp_count || to < 0 || to >= s_wp_count || from == to) return false;
    Waypoint tmp = s_wp[from];
    if (from < to) for (int j = from; j < to; j++) s_wp[j] = s_wp[j + 1];
    else           for (int j = from; j > to; j--) s_wp[j] = s_wp[j - 1];
    s_wp[to] = tmp;
    nav_stop();
    wp_save();
    return true;
}

bool wp_move_up(int i)   { return i > 0 && wp_move(i, i - 1); }
bool wp_move_down(int i) { return i >= 0 && i < s_wp_count - 1 && wp_move(i, i + 1); }

int wp_project(int i, float bearingDeg, float distM, const char *name) {
    if (i < 0 || i >= s_wp_count) return -1;
    const float R = 6371000.0f;
    float lat1 = radians(s_wp[i].lat), lon1 = radians(s_wp[i].lon);
    float b = radians(bearingDeg), d = distM / R;
    float lat2 = asinf(sinf(lat1) * cosf(d) + cosf(lat1) * sinf(d) * cosf(b));
    float lon2 = lon1 + atan2f(sinf(b) * sinf(d) * cosf(lat1), cosf(d) - sinf(lat1) * sinf(lat2));
    char nm[WP_NAME_LEN];
    if (name && name[0]) snprintf(nm, sizeof(nm), "%s", name);
    else                 snprintf(nm, sizeof(nm), "PRJ%02d", s_wp_count + 1);
    return wp_add_named(degrees(lat2), degrees(lon2), s_wp[i].alt, nm);
}

void wp_sort_nearest(float lat, float lon) {
    for (int a = 0; a < s_wp_count - 1; a++) {
        int best = a; float bd = wp_dist(lat, lon, s_wp[a].lat, s_wp[a].lon);
        for (int b = a + 1; b < s_wp_count; b++) {
            float dd = wp_dist(lat, lon, s_wp[b].lat, s_wp[b].lon);
            if (dd < bd) { bd = dd; best = b; }
        }
        if (best != a) { Waypoint t = s_wp[a]; s_wp[a] = s_wp[best]; s_wp[best] = t; }
    }
    nav_stop();
    wp_save();
}

void wp_sort_name() {
    for (int a = 0; a < s_wp_count - 1; a++) {
        int best = a;
        for (int b = a + 1; b < s_wp_count; b++)
            if (strcmp(s_wp[b].name, s_wp[best].name) < 0) best = b;
        if (best != a) { Waypoint t = s_wp[a]; s_wp[a] = s_wp[best]; s_wp[best] = t; }
    }
    nav_stop();
    wp_save();
}

// ---- navigation controller --------------------------------------------------

static NavMode s_navMode = NAV_NONE;
static int  s_navIndex   = -1; // current target waypoint index
static int  s_routeStart = -1; // first leg index (route mode)
static int  s_routeEnd   = -1; // last leg index (route mode)
static bool s_arrivedFinal = false;

void nav_goto(int i) {
    if (i < 0 || i >= s_wp_count) { nav_stop(); return; }
    s_navMode = NAV_GOTO; s_navIndex = i; s_arrivedFinal = false;
}

void nav_route_start(int from) {
    if (s_wp_count == 0) { nav_stop(); return; }
    if (from < 0) from = 0;
    if (from >= s_wp_count) from = s_wp_count - 1;
    s_navMode = NAV_ROUTE; s_routeStart = from; s_routeEnd = s_wp_count - 1;
    s_navIndex = from; s_arrivedFinal = false;
}

void nav_stop() { s_navMode = NAV_NONE; s_navIndex = -1; s_arrivedFinal = false; }
NavMode nav_mode() { return s_navMode; }
const Waypoint* nav_target() { return s_navMode == NAV_NONE ? nullptr : wp_get(s_navIndex); }
int nav_route_leg()   { return s_navMode == NAV_ROUTE ? s_navIndex - s_routeStart + 1 : 0; }
int nav_route_total() { return s_navMode == NAV_ROUTE ? s_routeEnd - s_routeStart + 1 : 0; }
bool nav_arrived_final() { return s_arrivedFinal; }

void nav_update(bool fix, float lat, float lon, float arriveM) {
    if (s_navMode == NAV_NONE || !fix) return;
    const Waypoint *t = wp_get(s_navIndex);
    if (!t) { nav_stop(); return; }
    if (wp_dist(lat, lon, t->lat, t->lon) <= arriveM) {
        if (s_navMode == NAV_ROUTE && s_navIndex < s_routeEnd) s_navIndex++; // next leg
        else s_arrivedFinal = true;
    }
}

// ---- track log --------------------------------------------------------------

void track_set_recording(bool on) { s_recording = on; }
bool track_is_recording() { return s_recording; }

int track_count() {
    return s_track_head_wrapped ? TRACK_MAX : s_track_count;
}

void track_clear() {
    s_track_count = 0;
    s_track_head = 0;
    s_track_head_wrapped = false;
}

void track_tick(bool valid, float lat, float lon, float alt,
                uint16_t year, uint8_t mon, uint8_t day,
                uint8_t hr, uint8_t min, uint8_t sec) {
    if (!s_recording || !valid) return;
    TrackPoint &p = s_track[s_track_head];
    p.lat = lat; p.lon = lon; p.alt = alt;
    p.year = year; p.mon = mon; p.day = day;
    p.hr = hr; p.min = min; p.sec = sec;
    p.rssi = (int16_t)lroundf(lora_last_rssi()); // geo-tag signal along the track
    s_track_head = (s_track_head + 1) % TRACK_MAX;
    if (s_track_head == 0) s_track_head_wrapped = true;
    if (!s_track_head_wrapped) s_track_count = s_track_head;
}

// Returns the i-th track point in chronological order, handling ring wrap.
static const TrackPoint* track_get(int i) {
    int n = track_count();
    if (i < 0 || i >= n) return nullptr;
    int start = s_track_head_wrapped ? s_track_head : 0;
    return &s_track[(start + i) % TRACK_MAX];
}

// ---- export -----------------------------------------------------------------

static void iso_time(const TrackPoint *p, char *buf, size_t cap) {
    snprintf(buf, cap, "%04u-%02u-%02uT%02u:%02u:%02uZ",
             p->year, p->mon, p->day, p->hr, p->min, p->sec);
}

void gps_export_gpx(Stream &out) {
    out.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    out.println("<gpx version=\"1.1\" creator=\"helrazr\" "
                "xmlns=\"http://www.topografix.com/GPX/1/1\">");
    for (int i = 0; i < s_wp_count; i++) {
        const Waypoint &w = s_wp[i];
        out.printf("  <wpt lat=\"%.6f\" lon=\"%.6f\">\n", w.lat, w.lon);
        out.printf("    <ele>%.1f</ele>\n", w.alt);
        out.printf("    <name>%s</name>\n", w.name);
        if (w.rssi != 0) out.printf("    <cmt>RSSI %d dBm SNR %d dB</cmt>\n", w.rssi, w.snr);
        out.println("  </wpt>");
    }
    int n = track_count();
    if (n > 0) {
        out.println("  <trk><name>track</name><trkseg>");
        char ts[24];
        for (int i = 0; i < n; i++) {
            const TrackPoint *p = track_get(i);
            out.printf("    <trkpt lat=\"%.6f\" lon=\"%.6f\">", p->lat, p->lon);
            out.printf("<ele>%.1f</ele>", p->alt);
            if (p->year > 0) { iso_time(p, ts, sizeof(ts)); out.printf("<time>%s</time>", ts); }
            if (p->rssi != 0) out.printf("<cmt>RSSI %d</cmt>", p->rssi);
            out.println("</trkpt>");
        }
        out.println("  </trkseg></trk>");
    }
    out.println("</gpx>");
}

void gps_export_csv(Stream &out) {
    out.println("type,name,lat,lon,alt,utc,rssi,snr");
    char ts[24];
    for (int i = 0; i < s_wp_count; i++) {
        const Waypoint &w = s_wp[i];
        out.printf("wpt,%s,%.6f,%.6f,%.1f,,%d,%d\n", w.name, w.lat, w.lon, w.alt, w.rssi, w.snr);
    }
    int n = track_count();
    for (int i = 0; i < n; i++) {
        const TrackPoint *p = track_get(i);
        if (p->year > 0) iso_time(p, ts, sizeof(ts)); else ts[0] = '\0';
        out.printf("trk,,%.6f,%.6f,%.1f,%s,%d,\n", p->lat, p->lon, p->alt, ts, p->rssi);
    }
}
