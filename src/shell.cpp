/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "shell.h"
#include "pins.h"
#include "display.h"
#include "gps.h"
#include "lora.h"
#include "waypoints.h"
#include "theme.h"
#include "modes.h"
#include "menu.h"

static char linebuf[128];
static int linepos = 0;

static float read_battery() {
    pinMode(PIN_BAT_ADC_EN, OUTPUT);
#if defined(HELTEC_V3) || defined(HELTEC_V4)
    digitalWrite(PIN_BAT_ADC_EN, LOW);
#else
    digitalWrite(PIN_BAT_ADC_EN, HIGH);
#endif
    delay(5);
#if defined(HELTEC_V3) || defined(HELTEC_V4)
    float voltage = (analogReadMilliVolts(PIN_BAT_ADC) / 1000.0f) * BAT_ADC_MULTIPLIER;
#else
    int raw = analogRead(PIN_BAT_ADC);
    float voltage = (raw / 1024.0f) * 3.6f * BAT_ADC_MULTIPLIER;
#endif
#if defined(HELTEC_V3) || defined(HELTEC_V4)
    digitalWrite(PIN_BAT_ADC_EN, HIGH);
#else
    digitalWrite(PIN_BAT_ADC_EN, LOW);
#endif
    return voltage;
}

static void cmd_status() {
    Serial.println("--- GPS ---");
    Serial.print("  Fix:  "); Serial.println(gps_has_fix() ? "3D" : "No");
    Serial.print("  Sats: "); Serial.println(gps_satellites());
    Serial.print("  Lat:  "); Serial.println(gps_latitude(), 6);
    Serial.print("  Lon:  "); Serial.println(gps_longitude(), 6);
    Serial.print("  Alt:  "); Serial.print(gps_altitude(), 1); Serial.println(" m");
    Serial.print("  Chars:"); Serial.println(gps_chars_processed());
    Serial.println("--- LoRa ---");
    Serial.print("  Rgn:  "); Serial.print(REGION_NAME);
    Serial.print(" ("); Serial.print(REGION_FREQ_START_MHZ, 1); Serial.print("-");
    Serial.print(REGION_FREQ_END_MHZ, 1); Serial.println(" MHz)");
    Serial.print("  Mode: "); Serial.println(lora_is_listening() ? "RX" : "Idle");
    Serial.print("  Freq: "); Serial.print(lora_frequency(), 1); Serial.println(" MHz");
    Serial.print("  BW:   "); Serial.print(lora_bandwidth(), 0); Serial.println(" kHz");
    Serial.print("  SF:   "); Serial.println(lora_spreading_factor());
    Serial.print("  RSSI: "); Serial.print(lora_last_rssi(), 1); Serial.println(" dBm");
    Serial.print("  SNR:  "); Serial.print(lora_last_snr(), 1); Serial.println(" dB");
    Serial.print("  Pkts: "); Serial.println(lora_packet_count());
    Serial.println("--- System ---");
    Serial.print("  Bat:  "); Serial.print(read_battery(), 2); Serial.println(" V");
    Serial.print("  Up:   "); Serial.print(millis() / 1000); Serial.println(" s");
    Serial.print("  Disp: "); Serial.println(display_is_on() ? "On" : "Off");
}

static void cmd_gps() {
    Serial.print("Fix:   "); Serial.println(gps_has_fix() ? "3D" : "No");
    Serial.print("Sats:  "); Serial.println(gps_satellites());
    Serial.print("Lat:   "); Serial.println(gps_latitude(), 6);
    Serial.print("Lon:   "); Serial.println(gps_longitude(), 6);
    Serial.print("Alt:   "); Serial.print(gps_altitude(), 1); Serial.println(" m");
    Serial.print("Speed: "); Serial.print(gps_speed_kmh(), 1); Serial.println(" km/h");
    Serial.print("Chars: "); Serial.println(gps_chars_processed());
}

static void cmd_lora() {
    Serial.print("Rgn:  "); Serial.print(REGION_NAME);
    Serial.print(" ("); Serial.print(REGION_FREQ_START_MHZ, 1); Serial.print("-");
    Serial.print(REGION_FREQ_END_MHZ, 1); Serial.println(" MHz)");
    Serial.print("Mode: "); Serial.println(lora_is_listening() ? "RX" : "Idle");
    Serial.print("Freq: "); Serial.print(lora_frequency(), 1); Serial.println(" MHz");
    Serial.print("BW:   "); Serial.print(lora_bandwidth(), 0); Serial.println(" kHz");
    Serial.print("SF:   "); Serial.println(lora_spreading_factor());
    Serial.print("RSSI: "); Serial.print(lora_last_rssi(), 1); Serial.println(" dBm");
    Serial.print("SNR:  "); Serial.print(lora_last_snr(), 1); Serial.println(" dB");
    Serial.print("Pkts: "); Serial.println(lora_packet_count());
}

// ---- waypoint import (multi-line) -------------------------------------------
// `wp import` / `wp import gpx` enter a mode where subsequent lines are parsed
// as waypoints until a line containing just "." (or "end").

static bool s_importMode = false;
static bool s_importGpx  = false;
static float s_gpxLat = 0, s_gpxLon = 0;
static char  s_gpxName[12];
static bool  s_gpxInWpt = false;

static void import_csv_line(char* line) {
    char buf[80]; strncpy(buf, line, sizeof(buf) - 1); buf[sizeof(buf) - 1] = '\0';
    char* tok[6]; int nt = 0;
    for (char* p = strtok(buf, ","); p && nt < 6; p = strtok(nullptr, ",")) tok[nt++] = p;
    if (nt < 3) return;
    const char* name; float lat, lon;
    if (strcmp(tok[0], "wpt") == 0 && nt >= 4) { name = tok[1]; lat = atof(tok[2]); lon = atof(tok[3]); }
    else if (strcmp(tok[0], "type") == 0 || strcmp(tok[0], "trk") == 0) return; // header / track row
    else { name = tok[0]; lat = atof(tok[1]); lon = atof(tok[2]); }
    if (lat == 0.0f && lon == 0.0f) return;
    int i = wp_add_named(lat, lon, 0.0f, name);
    if (i >= 0) Serial.printf("  + %s\r\n", wp_get(i)->name);
    else        Serial.println("  store full");
}

static void import_gpx_line(char* line) {
    char* w = strstr(line, "<wpt ");
    if (w) {
        char* la = strstr(w, "lat=\""); char* lo = strstr(w, "lon=\"");
        if (la && lo) { s_gpxLat = atof(la + 5); s_gpxLon = atof(lo + 5); s_gpxName[0] = '\0'; s_gpxInWpt = true; }
    }
    char* nm = strstr(line, "<name>");
    if (nm && s_gpxInWpt) {
        nm += 6; char* end = strstr(nm, "</name>");
        int len = end ? (int)(end - nm) : (int)strlen(nm);
        if (len > (int)sizeof(s_gpxName) - 1) len = sizeof(s_gpxName) - 1;
        strncpy(s_gpxName, nm, len); s_gpxName[len] = '\0';
    }
    if (strstr(line, "</wpt>") && s_gpxInWpt) {
        int i = wp_add_named(s_gpxLat, s_gpxLon, 0.0f, s_gpxName[0] ? s_gpxName : nullptr);
        s_gpxInWpt = false;
        if (i >= 0) Serial.printf("  + %s\r\n", wp_get(i)->name);
    }
}

static void import_line(char* line) {
    if (strcmp(line, ".") == 0 || strcmp(line, "end") == 0) {
        s_importMode = false; s_gpxInWpt = false;
        Serial.printf("Import done. %d waypoint(s) stored.\r\n", wp_count());
        return;
    }
    if (s_importGpx) import_gpx_line(line);
    else            import_csv_line(line);
}

// ---- command table -----------------------------------------------------------
// One table drives dispatch, help, usage, and Tab completion. To add a command:
// write a handler, then add one row below. `args` is the syntax for help/usage.

struct ShellCmd;
typedef void (*ShellFn)(const ShellCmd* c, const char* args);
struct ShellCmd { const char* name; const char* args; const char* help; ShellFn fn; };

static void shell_usage(const ShellCmd* c) {
    Serial.print("Usage: "); Serial.print(c->name);
    if (c->args && c->args[0]) { Serial.print(' '); Serial.print(c->args); }
    Serial.println();
}
static void print_help_table(); // defined after the table

// ---- handlers ---------------------------------------------------------------

static void h_help(const ShellCmd*, const char*)        { print_help_table(); }
static void h_status(const ShellCmd*, const char*)      { cmd_status(); }
static void h_gps(const ShellCmd*, const char*)         { cmd_gps(); }
static void h_gps_raw(const ShellCmd*, const char*)     { gps_cmd_raw(); }
static void h_gps_init(const ShellCmd*, const char*)    { gps_cmd_init(); }
static void h_gps_monitor(const ShellCmd*, const char*) { gps_cmd_monitor(); }
static void h_gps_test(const ShellCmd*, const char*)    { gps_diagnostic_test(); }
static void h_gps_export(const ShellCmd*, const char*)  { gps_export_gpx(Serial); }
static void h_gps_export_csv(const ShellCmd*, const char*) { gps_export_csv(Serial); }

static void h_wp_list(const ShellCmd*, const char*) {
    Serial.printf("%d waypoint(s):\r\n", wp_count());
    for (int i = 0; i < wp_count(); i++) {
        const Waypoint* w = wp_get(i);
        Serial.printf("  %d %-8s %.6f, %.6f  %.1fm  RSSI %d SNR %d\r\n",
                      i, w->name, w->lat, w->lon, w->alt, w->rssi, w->snr);
    }
}
static void h_wp_mark(const ShellCmd*, const char*) {
    if (!gps_has_fix()) { Serial.println("No fix; cannot mark"); return; }
    int idx = wp_add(gps_latitude(), gps_longitude(), gps_altitude());
    if (idx < 0) Serial.println("Waypoint store full");
    else Serial.printf("Marked %s\r\n", wp_get(idx)->name);
}
static void h_wp_add(const ShellCmd* c, const char* a) {
    float la, lo; char nm[12] = {0};
    int k = sscanf(a, "%f %f %11s", &la, &lo, nm);
    if (k < 2) { shell_usage(c); return; }
    int i = wp_add_named(la, lo, 0.0f, k >= 3 ? nm : nullptr);
    if (i < 0) Serial.println("Store full"); else Serial.printf("Added %s\r\n", wp_get(i)->name);
}
static void h_wp_del(const ShellCmd* c, const char* a) {
    if (!*a) { shell_usage(c); return; }
    Serial.println(wp_delete(atoi(a)) ? "Deleted" : "Bad index");
}
static void h_wp_rename(const ShellCmd* c, const char* a) {
    int i; char nm[12];
    if (sscanf(a, "%d %11s", &i, nm) == 2 && wp_rename(i, nm)) Serial.printf("Renamed %d -> %s\r\n", i, nm);
    else shell_usage(c);
}
static void h_wp_edit(const ShellCmd* c, const char* a) {
    int i = -1; float la, lo, al = 0;
    int k = sscanf(a, "%d %f %f %f", &i, &la, &lo, &al);
    const Waypoint* w = wp_get(i);
    if (k >= 3 && w && wp_edit(i, la, lo, k >= 4 ? al : w->alt)) Serial.println("Edited");
    else shell_usage(c);
}
static void h_wp_move(const ShellCmd* c, const char* a) {
    int x, y;
    if (sscanf(a, "%d %d", &x, &y) == 2 && wp_move(x, y)) Serial.printf("Moved %d -> %d\r\n", x, y);
    else shell_usage(c);
}
static void h_wp_project(const ShellCmd* c, const char* a) {
    int i; float brg, dist; char nm[12] = {0};
    int k = sscanf(a, "%d %f %f %11s", &i, &brg, &dist, nm);
    if (k < 3) { shell_usage(c); return; }
    int j = wp_project(i, brg, dist, k >= 4 ? nm : nullptr);
    if (j < 0) Serial.println("Bad index or full"); else Serial.printf("Projected %s\r\n", wp_get(j)->name);
}
static void h_wp_sort_near(const ShellCmd*, const char*) {
    if (gps_has_fix()) { wp_sort_nearest(gps_latitude(), gps_longitude()); Serial.println("Sorted by distance"); }
    else Serial.println("No fix");
}
static void h_wp_sort_name(const ShellCmd*, const char*) { wp_sort_name(); Serial.println("Sorted by name"); }
static void h_wp_clear(const ShellCmd*, const char*) {
    while (wp_count() > 0) wp_delete(0);
    Serial.println("Waypoints cleared");
}
static void h_wp_import(const ShellCmd*, const char* a) {
    s_importMode = true;
    s_importGpx = (strstr(a, "gpx") != nullptr);
    s_gpxInWpt = false;
    Serial.printf("Paste %s waypoints, then a line with just '.'\r\n",
                  s_importGpx ? "GPX" : "CSV (name,lat,lon)");
}
static void h_goto(const ShellCmd* c, const char* a) {
    if (!*a) { shell_usage(c); return; }
    int i = atoi(a);
    if (wp_get(i)) { nav_goto(i); Serial.printf("Go To %s\r\n", wp_get(i)->name); }
    else Serial.println("Bad index");
}
static void h_route_start(const ShellCmd*, const char* a) {
    int from = 0; sscanf(a, "%d", &from);
    nav_route_start(from);
    if (nav_mode() == NAV_ROUTE) Serial.printf("Route started, leg %d/%d\r\n", nav_route_leg(), nav_route_total());
    else Serial.println("No waypoints");
}
static void h_route_stop(const ShellCmd*, const char*) { nav_stop(); Serial.println("Navigation stopped"); }
static void h_route(const ShellCmd*, const char*) {
    if (nav_mode() == NAV_ROUTE) Serial.printf("Route: leg %d/%d -> %s\r\n", nav_route_leg(), nav_route_total(), nav_target()->name);
    else if (nav_mode() == NAV_GOTO) Serial.printf("GoTo: %s\r\n", nav_target()->name);
    else Serial.println("No active navigation");
}
static void h_track_start(const ShellCmd*, const char*) { track_set_recording(true);  Serial.println("Track recording ON"); }
static void h_track_stop(const ShellCmd*, const char*)  { track_set_recording(false); Serial.printf("Track recording OFF (%d points)\r\n", track_count()); }
static void h_track_clear(const ShellCmd*, const char*) { track_clear(); Serial.println("Track cleared"); }
static void h_track(const ShellCmd*, const char*) {
    Serial.printf("Track: %s, %d points\r\n", track_is_recording() ? "REC" : "stopped", track_count());
}
static void h_lora(const ShellCmd*, const char*) { cmd_lora(); }
static void h_lora_listen(const ShellCmd*, const char*) {
    if (lora_start_listen()) Serial.println("LoRa: listening"); else Serial.println("LoRa: failed to start");
}
static void h_lora_stop(const ShellCmd*, const char*) { lora_stop_listen(); Serial.println("LoRa: stopped"); }
static void h_lora_freq(const ShellCmd* c, const char* a) {
    if (!*a) { shell_usage(c); return; }
    float f = atof(a);
    if (lora_set_frequency(f)) { Serial.print("LoRa freq: "); Serial.print(f, 1); Serial.println(" MHz"); }
    else Serial.println("Failed to set frequency");
}
static void h_lora_bw(const ShellCmd* c, const char* a) {
    if (!*a) { shell_usage(c); return; }
    float bw = atof(a);
    if (lora_set_bandwidth(bw)) { Serial.print("LoRa BW: "); Serial.print(bw, 0); Serial.println(" kHz"); }
    else Serial.println("Failed to set bandwidth");
}
static void h_lora_sf(const ShellCmd* c, const char* a) {
    if (!*a) { shell_usage(c); return; }
    int sf = atoi(a);
    if (lora_set_spreading_factor(sf)) { Serial.print("LoRa SF: "); Serial.println(sf); }
    else Serial.println("SF must be 5-12");
}
static void h_theme(const ShellCmd*, const char* a) {
    if (strcmp(a, "next") == 0) theme_next();
    else if (*a) { for (int i = 0; i < theme_count(); i++) if (strcasecmp(a, theme_name(i)) == 0) { theme_set(i); break; } }
    Serial.printf("Theme: %s  [", theme_name(theme_current()));
    for (int i = 0; i < theme_count(); i++) Serial.printf("%s%s", i ? " " : "", theme_name(i));
    Serial.println("]");
    if (currentMode == MODE_MENU) { menu_init(); menu_draw(); } // re-skin now
}
static void h_display_on(const ShellCmd*, const char*)  { display_on();  Serial.println("Display on"); }
static void h_display_off(const ShellCmd*, const char*) { display_off(); Serial.println("Display off"); }
static void h_led_on(const ShellCmd*, const char*)  { digitalWrite(PIN_LED, LED_STATE_ON);  Serial.println("LED on"); }
static void h_led_off(const ShellCmd*, const char*) { digitalWrite(PIN_LED, LED_STATE_OFF); Serial.println("LED off"); }
static void h_bat(const ShellCmd*, const char*) { Serial.print("Battery: "); Serial.print(read_battery(), 2); Serial.println(" V"); }
static void h_reboot(const ShellCmd*, const char*) {
    Serial.println("Rebooting...");
    delay(100);
#if defined(HELTEC_T114)
    NVIC_SystemReset();
#elif defined(HELTEC_V3) || defined(HELTEC_V4)
    ESP.restart();
#endif
}

static const ShellCmd CMDS[] = {
    { "help",           "",                       "Show this help",           h_help },
    { "status",         "",                       "Show all status",          h_status },
    { "gps",            "",                       "GPS info",                 h_gps },
    { "gps raw",        "",                       "Dump raw GPS bytes (5s)",  h_gps_raw },
    { "gps init",       "",                       "Re-init M100 with debug",  h_gps_init },
    { "gps monitor",    "",                       "Live GPS data (30s)",      h_gps_monitor },
    { "gps test",       "",                       "Pin/baud diagnostic",      h_gps_test },
    { "gps export",     "",                       "Waypoints+track as GPX",   h_gps_export },
    { "gps export csv", "",                       "Waypoints+track as CSV",   h_gps_export_csv },
    { "wp list",        "",                       "List stored waypoints",    h_wp_list },
    { "wp mark",        "",                       "Mark current position",    h_wp_mark },
    { "wp add",         "<lat> <lon> [name]",     "Add by coordinates",       h_wp_add },
    { "wp del",         "<i>",                    "Delete waypoint i",        h_wp_del },
    { "wp rename",      "<i> <name>",             "Rename waypoint i",        h_wp_rename },
    { "wp edit",        "<i> <lat> <lon> [alt]",  "Edit coordinates",         h_wp_edit },
    { "wp move",        "<from> <to>",            "Reorder waypoint",         h_wp_move },
    { "wp project",     "<i> <brg> <dist> [name]","Project a new waypoint",   h_wp_project },
    { "wp sort near",   "",                       "Sort by distance",         h_wp_sort_near },
    { "wp sort name",   "",                       "Sort by name",             h_wp_sort_name },
    { "wp clear",       "",                       "Delete all waypoints",     h_wp_clear },
    { "wp import",      "[gpx]",                  "Import (paste, end '.')",  h_wp_import },
    { "goto",           "<i>",                    "Navigate to waypoint i",   h_goto },
    { "route start",    "[i]",                    "Start route navigation",   h_route_start },
    { "route stop",     "",                       "Stop navigation",          h_route_stop },
    { "route",          "",                       "Navigation status",        h_route },
    { "track start",    "",                       "Start track recording",    h_track_start },
    { "track stop",     "",                       "Stop track recording",     h_track_stop },
    { "track clear",    "",                       "Erase recorded track",     h_track_clear },
    { "track",          "",                       "Track status",             h_track },
    { "lora",           "",                       "LoRa info",                h_lora },
    { "lora listen",    "",                       "Start receiving",          h_lora_listen },
    { "lora stop",      "",                       "Stop receiving",           h_lora_stop },
    { "lora freq",      "<MHz>",                  "Set frequency",            h_lora_freq },
    { "lora bw",        "<kHz>",                  "Set bandwidth",            h_lora_bw },
    { "lora sf",        "<7-12>",                 "Set spreading factor",     h_lora_sf },
    { "theme",          "[name|next]",            "Set/cycle color theme",    h_theme },
    { "display on",     "",                       "Turn display on",          h_display_on },
    { "display off",    "",                       "Turn display off",         h_display_off },
    { "led on",         "",                       "LED on",                   h_led_on },
    { "led off",        "",                       "LED off",                  h_led_off },
    { "bat",            "",                       "Battery voltage",          h_bat },
    { "reboot",         "",                       "Software reset",           h_reboot },
};
static const int CMD_COUNT = sizeof(CMDS) / sizeof(CMDS[0]);

static void print_help_table() {
    Serial.println("Commands:");
    char left[44];
    for (int i = 0; i < CMD_COUNT; i++) {
        if (CMDS[i].args[0]) snprintf(left, sizeof(left), "%s %s", CMDS[i].name, CMDS[i].args);
        else                 snprintf(left, sizeof(left), "%s", CMDS[i].name);
        Serial.printf("  %-30s %s\r\n", left, CMDS[i].help);
    }
}

// Commands whose name starts with the typed prefix (for Tab/suggestions).
static int cmd_matches(const char* prefix, int len, const ShellCmd** out, int cap) {
    int n = 0;
    for (int i = 0; i < CMD_COUNT && n < cap; i++)
        if (strncmp(CMDS[i].name, prefix, len) == 0) out[n++] = &CMDS[i];
    return n;
}

// Tab completion: extend to the longest common prefix of matches; complete +
// add a space if unique; list candidates if it can't extend further.
static void shell_complete() {
    const ShellCmd* m[CMD_COUNT];
    int n = cmd_matches(linebuf, linepos, m, CMD_COUNT);
    if (n == 0) return;
    int lcp = strlen(m[0]->name);
    for (int i = 1; i < n; i++) {
        int j = 0;
        while (j < lcp && m[i]->name[j] == m[0]->name[j]) j++;
        lcp = j;
    }
    if (lcp > linepos) {                                  // extend toward the match
        while (linepos < lcp && linepos < (int)sizeof(linebuf) - 1) {
            char c = m[0]->name[linepos];
            linebuf[linepos++] = c;
            Serial.print(c);
        }
        if (n == 1 && linepos < (int)sizeof(linebuf) - 1) { // unique -> trailing space
            linebuf[linepos++] = ' ';
            Serial.print(' ');
        }
    } else {                                              // ambiguous -> list options
        Serial.println();
        for (int i = 0; i < n; i++) { Serial.print("  "); Serial.println(m[i]->name); }
        Serial.print(SHELL_PROMPT);
        linebuf[linepos] = '\0';
        Serial.print(linebuf);
    }
}

static void process_line(char* line) {
    // trim leading/trailing whitespace
    while (*line == ' ') line++;
    int len = strlen(line);
    while (len > 0 && line[len - 1] == ' ') line[--len] = '\0';
    if (len == 0) return;

    if (s_importMode) { import_line(line); return; } // routed to the importer
    if (strcmp(line, "?") == 0) { print_help_table(); return; }

    // Longest-prefix match so "lora freq" wins over "lora", etc.
    const ShellCmd* best = nullptr; int bestLen = 0;
    for (int i = 0; i < CMD_COUNT; i++) {
        int n = strlen(CMDS[i].name);
        if (strncmp(line, CMDS[i].name, n) == 0 && (line[n] == '\0' || line[n] == ' ') && n > bestLen) {
            best = &CMDS[i]; bestLen = n;
        }
    }
    if (best) {
        const char* a = line + bestLen;
        while (*a == ' ') a++;
        best->fn(best, a);
        return;
    }

    // Not a command: suggest prefixes (what Tab would offer), else unknown.
    const ShellCmd* m[CMD_COUNT];
    int n = cmd_matches(line, strlen(line), m, CMD_COUNT);
    if (n > 0) {
        Serial.println("Did you mean:");
        for (int i = 0; i < n; i++) { Serial.print("  "); Serial.println(m[i]->name); }
    } else {
        Serial.print("Unknown command: ");
        Serial.println(line);
        Serial.println("Type 'help' for commands");
    }
}

void shell_init() {
    Serial.begin(115200);
    // wait up to 3s for USB serial to connect
    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) {
        delay(10);
    }
    Serial.println();
    Serial.println("=== HelRazr Firmware ===");
    Serial.println("Type 'help' for commands");
    Serial.print(SHELL_PROMPT);
}

void shell_update() {
    static bool lastCR = false; // to swallow the LF of a CR+LF newline
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r' || c == '\n') {
            if (c == '\n' && lastCR) { lastCR = false; continue; } // CRLF -> one Enter
            lastCR = (c == '\r');
            Serial.println();
            if (linepos > 0) {
                linebuf[linepos] = '\0';
                process_line(linebuf);
                linepos = 0;
            }
            Serial.print(SHELL_PROMPT); // always re-prompt, even on a blank line
        } else if (c == '\t') {  // tab completion
            lastCR = false;
            if (!s_importMode) shell_complete();
        } else if (c == '\b' || c == 127) {  // backspace/delete
            lastCR = false;
            if (linepos > 0) {
                linepos--;
                Serial.print("\b \b");
            }
        } else if (linepos < (int)sizeof(linebuf) - 1) {
            lastCR = false;
            linebuf[linepos++] = c;
            Serial.print(c);  // echo
        }
    }
}
