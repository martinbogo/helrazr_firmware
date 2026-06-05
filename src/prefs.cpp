/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "prefs.h"
#include <math.h>

#if defined(ESP32)
#include <Preferences.h>
static Preferences prefsStore;
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;
static const char *PREFS_PATH = "/prefs.dat";
#endif

struct PrefsBlob {
    uint8_t units;
    uint8_t coords;
    uint8_t compassMode;
    uint8_t brightness; // 0-100
};
static PrefsBlob g = { UNITS_METRIC, COORD_DEG, 0, 100 };

static void load() {
#if defined(ESP32)
    prefsStore.begin("gpsprefs", true);
    prefsStore.getBytes("p", &g, sizeof(g));
    prefsStore.end();
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
    InternalFS.begin();
    File f(InternalFS);
    if (f.open(PREFS_PATH, FILE_O_READ)) {
        f.read((uint8_t *)&g, sizeof(g));
        f.close();
    }
#endif
    if (g.units >= UNITS_COUNT)  g.units = UNITS_METRIC;
    if (g.coords >= COORD_COUNT) g.coords = COORD_DEG;
    if (g.brightness == 0 || g.brightness > 100) g.brightness = 100;
}

static void store() {
#if defined(ESP32)
    prefsStore.begin("gpsprefs", false);
    prefsStore.putBytes("p", &g, sizeof(g));
    prefsStore.end();
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
    InternalFS.remove(PREFS_PATH);
    File f(InternalFS);
    if (f.open(PREFS_PATH, FILE_O_WRITE)) {
        f.write((const uint8_t *)&g, sizeof(g));
        f.close();
    }
#endif
}

void prefs_init() { load(); }

int  prefs_units()        { return g.units; }
int  prefs_coords()       { return g.coords; }
int  prefs_compass_mode() { return g.compassMode; }
int  prefs_brightness()   { return g.brightness; }

void prefs_set_units(int u)        { g.units = (uint8_t)(u % UNITS_COUNT); store(); }
void prefs_set_coords(int c)       { g.coords = (uint8_t)(c % COORD_COUNT); store(); }
void prefs_set_compass_mode(int m) { g.compassMode = (uint8_t)m; store(); }
void prefs_set_brightness(int b)   { if (b < 5) b = 5; if (b > 100) b = 100; g.brightness = (uint8_t)b; store(); }

const char* units_name(int u)  { return u == UNITS_IMPERIAL ? "Imperial" : "Metric"; }
const char* coords_name(int c) {
    switch (c) { case COORD_GRID: return "Grid"; case COORD_DDM: return "Deg Min"; default: return "Degrees"; }
}

// ---- formatters -------------------------------------------------------------

void fmt_speed(float kmh, char *buf, size_t n) {
    if (prefs_units() == UNITS_IMPERIAL) snprintf(buf, n, "%.1f mph", kmh * 0.621371f);
    else                                 snprintf(buf, n, "%.1f km/h", kmh);
}

void fmt_alt(float meters, char *buf, size_t n) {
    if (prefs_units() == UNITS_IMPERIAL) snprintf(buf, n, "%dft", (int)lroundf(meters * 3.28084f));
    else                                 snprintf(buf, n, "%dm", (int)lroundf(meters));
}

void fmt_dist(float meters, char *buf, size_t n) {
    if (prefs_units() == UNITS_IMPERIAL) {
        float ft = meters * 3.28084f;
        if (ft < 1000.0f) snprintf(buf, n, "%dft", (int)lroundf(ft));
        else              snprintf(buf, n, "%.2fmi", meters / 1609.344f);
    } else {
        if (meters < 1000.0f) snprintf(buf, n, "%dm", (int)lroundf(meters));
        else                  snprintf(buf, n, "%.2fkm", meters / 1000.0f);
    }
}

// 6-character Maidenhead grid locator (used by ham operators).
void maidenhead(float lat, float lon, char *buf, size_t n) {
    float lo = lon + 180.0f, la = lat + 90.0f;
    int A = (int)(lo / 20); lo -= A * 20;
    int B = (int)(la / 10); la -= B * 10;
    int C = (int)(lo / 2);  lo -= C * 2;
    int D = (int)(la / 1);  la -= D * 1;
    int E = (int)(lo / (2.0f / 24));
    int F = (int)(la / (1.0f / 24));
    if (A < 0) A = 0; if (A > 17) A = 17;
    if (B < 0) B = 0; if (B > 17) B = 17;
    snprintf(buf, n, "%c%c%d%d%c%c", 'A' + A, 'A' + B, C, D, 'a' + E, 'a' + F);
}

void fmt_position(float lat, float lon, char *buf, size_t n) {
    switch (prefs_coords()) {
        case COORD_GRID:
            maidenhead(lat, lon, buf, n);
            break;
        case COORD_DDM: {
            char ns = lat >= 0 ? 'N' : 'S', ew = lon >= 0 ? 'E' : 'W';
            float a = fabsf(lat), o = fabsf(lon);
            int ad = (int)a, od = (int)o;
            snprintf(buf, n, "%d %05.2f%c %d %05.2f%c",
                     ad, (a - ad) * 60.0f, ns, od, (o - od) * 60.0f, ew);
            break;
        }
        default:
            snprintf(buf, n, "%.4f, %.4f", lat, lon);
            break;
    }
}
