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

static void print_help() {
    Serial.println("Commands:");
    Serial.println("  help              Show this help");
    Serial.println("  status            Show all status");
    Serial.println("  gps               GPS info");
    Serial.println("  gps raw           Dump raw GPS bytes (5s)");
    Serial.println("  gps init          Re-init M100 with debug");
    Serial.println("  gps monitor       Live GPS data (30s)");
    Serial.println("  gps test          Pin/baud diagnostic");
    Serial.println("  gps export        Dump waypoints+track as GPX");
    Serial.println("  gps export csv    Dump waypoints+track as CSV");
    Serial.println("  wp list           List stored waypoints");
    Serial.println("  wp mark           Mark current position");
    Serial.println("  wp add <la> <lo> [name]  Add by coordinates");
    Serial.println("  wp del <i>        Delete waypoint i");
    Serial.println("  wp rename <i> <name>     Rename waypoint i");
    Serial.println("  wp edit <i> <la> <lo> [alt]  Edit coords");
    Serial.println("  wp move <from> <to>      Reorder waypoint");
    Serial.println("  wp project <i> <brg> <dist> [name]  Project new wp");
    Serial.println("  wp sort near|name Reorder list");
    Serial.println("  wp clear          Delete all waypoints");
    Serial.println("  wp import [gpx]    Import waypoints (paste, end with '.')");
    Serial.println("  goto <i>          Navigate to waypoint i");
    Serial.println("  route start [i]   Start route navigation");
    Serial.println("  route stop        Stop navigation");
    Serial.println("  track start|stop  Toggle track recording");
    Serial.println("  track clear       Erase recorded track");
    Serial.println("  lora              LoRa info");
    Serial.println("  lora listen       Start receiving");
    Serial.println("  lora stop         Stop receiving");
    Serial.println("  lora freq <MHz>   Set frequency");
    Serial.println("  lora bw <kHz>     Set bandwidth");
    Serial.println("  lora sf <7-12>    Set spreading factor");
    Serial.println("  theme [name|next] Set/cycle color theme");
    Serial.println("  display on        Turn display on");
    Serial.println("  display off       Turn display off");
    Serial.println("  led on|off        Toggle LED");
    Serial.println("  bat               Battery voltage");
    Serial.println("  reboot            Software reset");
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
    Serial.print("Mode: "); Serial.println(lora_is_listening() ? "RX" : "Idle");
    Serial.print("Freq: "); Serial.print(lora_frequency(), 1); Serial.println(" MHz");
    Serial.print("BW:   "); Serial.print(lora_bandwidth(), 0); Serial.println(" kHz");
    Serial.print("SF:   "); Serial.println(lora_spreading_factor());
    Serial.print("RSSI: "); Serial.print(lora_last_rssi(), 1); Serial.println(" dBm");
    Serial.print("SNR:  "); Serial.print(lora_last_snr(), 1); Serial.println(" dB");
    Serial.print("Pkts: "); Serial.println(lora_packet_count());
}

static bool startsWith(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

// Pointer to the arguments after `cmd` in `line`, skipping spaces ("" if none).
static const char* args_of(const char* line, const char* cmd) {
    const char* p = line + strlen(cmd);
    while (*p == ' ') p++;
    return p;
}

// Canonical command list -- drives Tab completion and "did you mean" hints.
static const char* COMMANDS[] = {
    "help", "status",
    "gps", "gps raw", "gps init", "gps monitor", "gps test",
    "gps export", "gps export csv",
    "wp list", "wp mark", "wp add", "wp del", "wp rename", "wp edit",
    "wp move", "wp project", "wp sort near", "wp sort name", "wp clear", "wp import",
    "goto", "route start", "route stop", "route",
    "track start", "track stop", "track clear", "track",
    "lora", "lora listen", "lora stop", "lora freq", "lora bw", "lora sf",
    "theme", "display on", "display off", "led on", "led off", "bat", "reboot",
};
static const int CMD_COUNT = sizeof(COMMANDS) / sizeof(COMMANDS[0]);

// Collects commands that start with the current input. Returns the count.
static int cmd_matches(const char* prefix, int len, const char** out, int cap) {
    int n = 0;
    for (int i = 0; i < CMD_COUNT && n < cap; i++)
        if (strncmp(COMMANDS[i], prefix, len) == 0) out[n++] = COMMANDS[i];
    return n;
}

// Tab completion: extend to the longest common prefix of matches; complete +
// add a space if unique; list candidates if it can't extend further.
static void shell_complete() {
    const char* m[CMD_COUNT];
    int n = cmd_matches(linebuf, linepos, m, CMD_COUNT);
    if (n == 0) return;
    int lcp = strlen(m[0]);
    for (int i = 1; i < n; i++) {
        int j = 0;
        while (j < lcp && m[i][j] == m[0][j]) j++;
        lcp = j;
    }
    if (lcp > linepos) {                                  // extend toward the match
        while (linepos < lcp && linepos < (int)sizeof(linebuf) - 1) {
            char c = m[0][linepos];
            linebuf[linepos++] = c;
            Serial.print(c);
        }
        if (n == 1 && linepos < (int)sizeof(linebuf) - 1) { // unique -> trailing space
            linebuf[linepos++] = ' ';
            Serial.print(' ');
        }
    } else {                                              // ambiguous -> list options
        Serial.println();
        for (int i = 0; i < n; i++) { Serial.print("  "); Serial.println(m[i]); }
        Serial.print(SHELL_PROMPT);
        linebuf[linepos] = '\0';
        Serial.print(linebuf);
    }
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

static void process_line(char* line) {
    // trim leading/trailing whitespace
    while (*line == ' ') line++;
    int len = strlen(line);
    while (len > 0 && line[len - 1] == ' ') line[--len] = '\0';
    if (len == 0) return;

    if (s_importMode) { import_line(line); return; } // routed to the importer

    if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
        print_help();
    } else if (strcmp(line, "status") == 0) {
        cmd_status();
    } else if (strcmp(line, "gps") == 0) {
        cmd_gps();
    } else if (strcmp(line, "gps test") == 0) {
        gps_diagnostic_test();
    } else if (strcmp(line, "gps raw") == 0) {
        gps_cmd_raw();
    } else if (strcmp(line, "gps init") == 0) {
        gps_cmd_init();
    } else if (strcmp(line, "gps monitor") == 0) {
        gps_cmd_monitor();
    } else if (strcmp(line, "gps export") == 0) {
        gps_export_gpx(Serial);
    } else if (strcmp(line, "gps export csv") == 0) {
        gps_export_csv(Serial);
    } else if (strcmp(line, "wp list") == 0) {
        Serial.printf("%d waypoint(s):\r\n", wp_count());
        for (int i = 0; i < wp_count(); i++) {
            const Waypoint* w = wp_get(i);
            Serial.printf("  %d %-8s %.6f, %.6f  %.1fm  RSSI %d SNR %d\r\n",
                          i, w->name, w->lat, w->lon, w->alt, w->rssi, w->snr);
        }
    } else if (strcmp(line, "wp mark") == 0) {
        if (!gps_has_fix()) {
            Serial.println("No fix; cannot mark");
        } else {
            int idx = wp_add(gps_latitude(), gps_longitude(), gps_altitude());
            if (idx < 0) Serial.println("Waypoint store full");
            else Serial.printf("Marked %s\r\n", wp_get(idx)->name);
        }
    } else if (strcmp(line, "wp clear") == 0) {
        while (wp_count() > 0) wp_delete(0);
        Serial.println("Waypoints cleared");
    } else if (strcmp(line, "wp import") == 0 || strcmp(line, "wp import gpx") == 0) {
        s_importMode = true;
        s_importGpx = (strstr(line, "gpx") != nullptr);
        s_gpxInWpt = false;
        Serial.printf("Paste %s waypoints, then a line with just '.'\r\n",
                      s_importGpx ? "GPX" : "CSV (name,lat,lon)");
    } else if (startsWith(line, "wp add")) {
        const char* a = args_of(line, "wp add");
        float la, lo; char nm[12] = {0};
        int k = sscanf(a, "%f %f %11s", &la, &lo, nm);
        if (k >= 2) { int i = wp_add_named(la, lo, 0.0f, k >= 3 ? nm : nullptr);
            if (i < 0) Serial.println("Store full"); else Serial.printf("Added %s\r\n", wp_get(i)->name); }
        else Serial.println("Usage: wp add <lat> <lon> [name]");
    } else if (startsWith(line, "wp del")) {
        const char* a = args_of(line, "wp del");
        if (!*a) Serial.println("Usage: wp del <i>");
        else Serial.println(wp_delete(atoi(a)) ? "Deleted" : "Bad index");
    } else if (startsWith(line, "wp rename")) {
        const char* a = args_of(line, "wp rename");
        int i; char nm[12];
        if (sscanf(a, "%d %11s", &i, nm) == 2 && wp_rename(i, nm)) Serial.printf("Renamed %d -> %s\r\n", i, nm);
        else Serial.println("Usage: wp rename <i> <name>");
    } else if (startsWith(line, "wp edit")) {
        const char* a = args_of(line, "wp edit");
        int i = -1; float la, lo, al = 0;
        int k = sscanf(a, "%d %f %f %f", &i, &la, &lo, &al);
        const Waypoint* w = wp_get(i);
        if (k >= 3 && w && wp_edit(i, la, lo, k >= 4 ? al : w->alt)) Serial.println("Edited");
        else Serial.println("Usage: wp edit <i> <lat> <lon> [alt]");
    } else if (startsWith(line, "wp move")) {
        const char* a = args_of(line, "wp move");
        int x, y;
        if (sscanf(a, "%d %d", &x, &y) == 2 && wp_move(x, y)) Serial.printf("Moved %d -> %d\r\n", x, y);
        else Serial.println("Usage: wp move <from> <to>");
    } else if (startsWith(line, "wp project")) {
        const char* a = args_of(line, "wp project");
        int i; float brg, dist; char nm[12] = {0};
        int k = sscanf(a, "%d %f %f %11s", &i, &brg, &dist, nm);
        if (k >= 3) { int j = wp_project(i, brg, dist, k >= 4 ? nm : nullptr);
            if (j < 0) Serial.println("Bad index or full"); else Serial.printf("Projected %s\r\n", wp_get(j)->name); }
        else Serial.println("Usage: wp project <i> <brg> <dist_m> [name]");
    } else if (strcmp(line, "wp sort near") == 0) {
        if (gps_has_fix()) { wp_sort_nearest(gps_latitude(), gps_longitude()); Serial.println("Sorted by distance"); }
        else Serial.println("No fix");
    } else if (strcmp(line, "wp sort name") == 0) {
        wp_sort_name(); Serial.println("Sorted by name");
    } else if (startsWith(line, "goto")) {
        const char* a = args_of(line, "goto");
        if (!*a) Serial.println("Usage: goto <i>");
        else { int i = atoi(a);
            if (wp_get(i)) { nav_goto(i); Serial.printf("Go To %s\r\n", wp_get(i)->name); }
            else Serial.println("Bad index"); }
    } else if (startsWith(line, "route start")) {
        int from = 0; sscanf(args_of(line, "route start"), "%d", &from);
        nav_route_start(from);
        if (nav_mode() == NAV_ROUTE) Serial.printf("Route started, leg %d/%d\r\n", nav_route_leg(), nav_route_total());
        else Serial.println("No waypoints");
    } else if (strcmp(line, "route stop") == 0) {
        nav_stop(); Serial.println("Navigation stopped");
    } else if (strcmp(line, "route") == 0) {
        if (nav_mode() == NAV_ROUTE) Serial.printf("Route: leg %d/%d -> %s\r\n", nav_route_leg(), nav_route_total(), nav_target()->name);
        else if (nav_mode() == NAV_GOTO) Serial.printf("GoTo: %s\r\n", nav_target()->name);
        else Serial.println("No active navigation");
    } else if (strcmp(line, "track start") == 0) {
        track_set_recording(true);
        Serial.println("Track recording ON");
    } else if (strcmp(line, "track stop") == 0) {
        track_set_recording(false);
        Serial.printf("Track recording OFF (%d points)\r\n", track_count());
    } else if (strcmp(line, "track clear") == 0) {
        track_clear();
        Serial.println("Track cleared");
    } else if (strcmp(line, "track") == 0) {
        Serial.printf("Track: %s, %d points\r\n",
                      track_is_recording() ? "REC" : "stopped", track_count());
    } else if (strcmp(line, "lora") == 0) {
        cmd_lora();
    } else if (strcmp(line, "lora listen") == 0) {
        if (lora_start_listen()) Serial.println("LoRa: listening");
        else Serial.println("LoRa: failed to start");
    } else if (strcmp(line, "lora stop") == 0) {
        lora_stop_listen();
        Serial.println("LoRa: stopped");
    } else if (startsWith(line, "lora freq ")) {
        float f = atof(line + 10);
        if (lora_set_frequency(f)) {
            Serial.print("LoRa freq: "); Serial.print(f, 1); Serial.println(" MHz");
        } else {
            Serial.println("Failed to set frequency");
        }
    } else if (startsWith(line, "lora bw ")) {
        float bw = atof(line + 8);
        if (lora_set_bandwidth(bw)) {
            Serial.print("LoRa BW: "); Serial.print(bw, 0); Serial.println(" kHz");
        } else {
            Serial.println("Failed to set bandwidth");
        }
    } else if (startsWith(line, "lora sf ")) {
        int sf = atoi(line + 8);
        if (lora_set_spreading_factor(sf)) {
            Serial.print("LoRa SF: "); Serial.println(sf);
        } else {
            Serial.println("SF must be 5-12");
        }
    } else if (startsWith(line, "theme")) {
        const char *arg = line + 5;
        while (*arg == ' ') arg++;
        if (strcmp(arg, "next") == 0) {
            theme_next();
        } else if (*arg) {
            for (int i = 0; i < theme_count(); i++)
                if (strcasecmp(arg, theme_name(i)) == 0) { theme_set(i); break; }
        }
        Serial.printf("Theme: %s  [", theme_name(theme_current()));
        for (int i = 0; i < theme_count(); i++) Serial.printf("%s%s", i ? " " : "", theme_name(i));
        Serial.println("]");
        if (currentMode == MODE_MENU) { menu_init(); menu_draw(); } // re-skin now
    } else if (strcmp(line, "display on") == 0) {
        display_on();
        Serial.println("Display on");
    } else if (strcmp(line, "display off") == 0) {
        display_off();
        Serial.println("Display off");
    } else if (strcmp(line, "led on") == 0) {
        digitalWrite(PIN_LED, LED_STATE_ON);
        Serial.println("LED on");
    } else if (strcmp(line, "led off") == 0) {
        digitalWrite(PIN_LED, LED_STATE_OFF);
        Serial.println("LED off");
    } else if (strcmp(line, "bat") == 0) {
        Serial.print("Battery: "); Serial.print(read_battery(), 2); Serial.println(" V");
    } else if (strcmp(line, "reboot") == 0) {
        Serial.println("Rebooting...");
        delay(100);
#if defined(HELTEC_T114)
        NVIC_SystemReset();
#elif defined(HELTEC_V3) || defined(HELTEC_V4)
        ESP.restart();
#endif
    } else {
        // If the input is a prefix of known commands, suggest them (Tab would
        // have completed/listed these); otherwise it's genuinely unknown.
        const char* m[CMD_COUNT];
        int n = cmd_matches(line, strlen(line), m, CMD_COUNT);
        if (n > 0) {
            Serial.println("Did you mean:");
            for (int i = 0; i < n; i++) { Serial.print("  "); Serial.println(m[i]); }
        } else {
            Serial.print("Unknown command: ");
            Serial.println(line);
            Serial.println("Type 'help' for commands");
        }
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
