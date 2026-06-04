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

static void process_line(char* line) {
    // trim leading/trailing whitespace
    while (*line == ' ') line++;
    int len = strlen(line);
    while (len > 0 && line[len - 1] == ' ') line[--len] = '\0';
    if (len == 0) return;

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
        Serial.printf("%d waypoint(s):\n", wp_count());
        for (int i = 0; i < wp_count(); i++) {
            const Waypoint* w = wp_get(i);
            Serial.printf("  %d %-8s %.6f, %.6f  %.1fm\n", i, w->name, w->lat, w->lon, w->alt);
        }
    } else if (strcmp(line, "wp mark") == 0) {
        if (!gps_has_fix()) {
            Serial.println("No fix; cannot mark");
        } else {
            int idx = wp_add(gps_latitude(), gps_longitude(), gps_altitude());
            if (idx < 0) Serial.println("Waypoint store full");
            else Serial.printf("Marked %s\n", wp_get(idx)->name);
        }
    } else if (strcmp(line, "wp clear") == 0) {
        while (wp_count() > 0) wp_delete(0);
        Serial.println("Waypoints cleared");
    } else if (startsWith(line, "wp add ")) {
        float la, lo; char nm[12] = {0};
        int k = sscanf(line + 7, "%f %f %11s", &la, &lo, nm);
        if (k >= 2) { int i = wp_add_named(la, lo, 0.0f, k >= 3 ? nm : nullptr);
            if (i < 0) Serial.println("Store full"); else Serial.printf("Added %s\n", wp_get(i)->name); }
        else Serial.println("Usage: wp add <lat> <lon> [name]");
    } else if (startsWith(line, "wp del ")) {
        int i = atoi(line + 7);
        Serial.println(wp_delete(i) ? "Deleted" : "Bad index");
    } else if (startsWith(line, "wp rename ")) {
        int i; char nm[12];
        if (sscanf(line + 10, "%d %11s", &i, nm) == 2 && wp_rename(i, nm)) Serial.printf("Renamed %d -> %s\n", i, nm);
        else Serial.println("Usage: wp rename <i> <name>");
    } else if (startsWith(line, "wp edit ")) {
        int i; float la, lo, al = 0;
        int k = sscanf(line + 8, "%d %f %f %f", &i, &la, &lo, &al);
        const Waypoint* w = wp_get(i);
        if (k >= 3 && w && wp_edit(i, la, lo, k >= 4 ? al : w->alt)) Serial.println("Edited");
        else Serial.println("Usage: wp edit <i> <lat> <lon> [alt]");
    } else if (startsWith(line, "wp move ")) {
        int a, b;
        if (sscanf(line + 8, "%d %d", &a, &b) == 2 && wp_move(a, b)) Serial.printf("Moved %d -> %d\n", a, b);
        else Serial.println("Usage: wp move <from> <to>");
    } else if (startsWith(line, "wp project ")) {
        int i; float brg, dist; char nm[12] = {0};
        int k = sscanf(line + 11, "%d %f %f %11s", &i, &brg, &dist, nm);
        if (k >= 3) { int j = wp_project(i, brg, dist, k >= 4 ? nm : nullptr);
            if (j < 0) Serial.println("Bad index or full"); else Serial.printf("Projected %s\n", wp_get(j)->name); }
        else Serial.println("Usage: wp project <i> <brg> <dist_m> [name]");
    } else if (strcmp(line, "wp sort near") == 0) {
        if (gps_has_fix()) { wp_sort_nearest(gps_latitude(), gps_longitude()); Serial.println("Sorted by distance"); }
        else Serial.println("No fix");
    } else if (strcmp(line, "wp sort name") == 0) {
        wp_sort_name(); Serial.println("Sorted by name");
    } else if (startsWith(line, "goto ")) {
        int i = atoi(line + 5);
        if (wp_get(i)) { nav_goto(i); Serial.printf("Go To %s\n", wp_get(i)->name); }
        else Serial.println("Bad index");
    } else if (startsWith(line, "route start")) {
        int from = 0; sscanf(line + 11, "%d", &from);
        nav_route_start(from);
        if (nav_mode() == NAV_ROUTE) Serial.printf("Route started, leg %d/%d\n", nav_route_leg(), nav_route_total());
        else Serial.println("No waypoints");
    } else if (strcmp(line, "route stop") == 0) {
        nav_stop(); Serial.println("Navigation stopped");
    } else if (strcmp(line, "route") == 0) {
        if (nav_mode() == NAV_ROUTE) Serial.printf("Route: leg %d/%d -> %s\n", nav_route_leg(), nav_route_total(), nav_target()->name);
        else if (nav_mode() == NAV_GOTO) Serial.printf("GoTo: %s\n", nav_target()->name);
        else Serial.println("No active navigation");
    } else if (strcmp(line, "track start") == 0) {
        track_set_recording(true);
        Serial.println("Track recording ON");
    } else if (strcmp(line, "track stop") == 0) {
        track_set_recording(false);
        Serial.printf("Track recording OFF (%d points)\n", track_count());
    } else if (strcmp(line, "track clear") == 0) {
        track_clear();
        Serial.println("Track cleared");
    } else if (strcmp(line, "track") == 0) {
        Serial.printf("Track: %s, %d points\n",
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
    Serial.print("> ");
}

void shell_update() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r' || c == '\n') {
            if (linepos > 0) {
                Serial.println();
                linebuf[linepos] = '\0';
                process_line(linebuf);
                linepos = 0;
                Serial.print("> ");
            }
        } else if (c == '\b' || c == 127) {  // backspace/delete
            if (linepos > 0) {
                linepos--;
                Serial.print("\b \b");
            }
        } else if (linepos < (int)sizeof(linebuf) - 1) {
            linebuf[linepos++] = c;
            Serial.print(c);  // echo
        }
    }
}
