/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "monitor.h"
#include "lora.h"
#include "modes.h"
#include "display.h"




static const uint32_t DWELL_MS = 5000;

struct ChannelStats { int packets; float lastRSSI; uint32_t lastSeenMs; };
static ChannelStats stats[MESH_CHANNEL_COUNT];
static int curChan = 0;
static uint32_t dwellStart = 0;

static uint16_t rssiColor(float rssi) {
    if (rssi >= -80)  return DISPLAY_RED;
    if (rssi >= -100) return DISPLAY_YELLOW;
    return DISPLAY_GREEN;
}

static void drawTable() {
    display_clear();
#if HAS_OLED
    display_draw_text_abs(15, 0, DISPLAY_CYAN, "Channel Monitor");
    display_draw_hline(0, 10, 128, DISPLAY_GRAY);
    
    // Column headers (small font)
    display_draw_text_small_abs(0,   12, DISPLAY_CYAN, "Chan      Pk  RSSI Age");
#else
    display_draw_text_abs(60, 15, DISPLAY_CYAN, "Channel Monitor");
    display_draw_hline(0, 20, 240, DISPLAY_GRAY);
    display_draw_text_small_abs(0,   30, DISPLAY_CYAN, "Channel");
    display_draw_text_small_abs(90,  30, DISPLAY_CYAN, "Pkts");
    display_draw_text_small_abs(128, 30, DISPLAY_CYAN, "RSSI");
    display_draw_text_small_abs(178, 30, DISPLAY_CYAN, "Last");
#endif

    uint32_t now = millis();
#if HAS_OLED
    int start = (curChan >= 4) ? curChan - 3 : 0;
    for (int i = start; i < start + 4 && i < MESH_CHANNEL_COUNT; i++) {
        int y = 22 + (i - start) * 8;
#else
    for (int i = 0; i < MESH_CHANNEL_COUNT; i++) {
        int y = 40 + i * 11;
#endif
        bool active = (i == curChan);
        uint16_t col = active ? DISPLAY_YELLOW : DISPLAY_WHITE;

        char rssiStr[8], ageStr[8];
        if (stats[i].packets == 0) {
#if HAS_OLED
            snprintf(rssiStr, sizeof(rssiStr), "-- ");
#else
            snprintf(rssiStr, sizeof(rssiStr), "  --");
#endif
            snprintf(ageStr,  sizeof(ageStr),  " --");
        } else {
#if HAS_OLED
            snprintf(rssiStr, sizeof(rssiStr), "%3d", (int)stats[i].lastRSSI);
#else
            snprintf(rssiStr, sizeof(rssiStr), "%4d", (int)stats[i].lastRSSI);
#endif
            uint32_t age = (now - stats[i].lastSeenMs) / 1000;
            if (age < 60) snprintf(ageStr, sizeof(ageStr), "%3lus", age);
            else          snprintf(ageStr, sizeof(ageStr), "%3lum", age / 60);
        }

#if HAS_OLED
        char row[40];
        snprintf(row, sizeof(row), "%-9s %3d %s", MESH_CHANNELS[i].name, stats[i].packets, rssiStr);
        display_draw_text_small_abs(0, y, col, row);
        display_draw_text_small_abs(105, y, DISPLAY_CYAN, ageStr);
#else
        display_draw_text_small_abs(0,   y, col, MESH_CHANNELS[i].name);
        char pkts[6]; snprintf(pkts, sizeof(pkts), "%4d", stats[i].packets);
        display_draw_text_small_abs(90,  y, col, pkts);
        display_draw_text_small_abs(128, y, stats[i].packets ? rssiColor(stats[i].lastRSSI) : (uint16_t)DISPLAY_CYAN, rssiStr);
        display_draw_text_small_abs(178, y, DISPLAY_CYAN, ageStr);
#endif
    }

    char foot[32];
#if HAS_OLED
    snprintf(foot, sizeof(foot), "RX: %-10s", MESH_CHANNELS[curChan].name);
    display_draw_text_small_abs(0, 56, DISPLAY_YELLOW, foot);
#else
    snprintf(foot, sizeof(foot), "Listening: %-10s", MESH_CHANNELS[curChan].name);
    display_draw_text_small_abs(0, 126, DISPLAY_YELLOW, foot);
#endif
}

void monitor_enter() {
    memset(stats, 0, sizeof(stats));
    curChan = 0; dwellStart = millis();
    lora_apply_channel(0);
    lora_start_listen();
    drawTable();
}

void monitor_update() {
    if (lora_poll_packet()) {
        stats[curChan].packets++;
        stats[curChan].lastRSSI   = lora_last_rssi();
        stats[curChan].lastSeenMs = millis();
        Serial.print("Monitor ["); Serial.print(MESH_CHANNELS[curChan].name);
        Serial.print("] RSSI="); Serial.print(lora_last_rssi());
        Serial.print(" SNR="); Serial.println(lora_last_snr());
        drawTable();
    }

    uint32_t now = millis();
    if (dwellStart == 0 || (now - dwellStart >= DWELL_MS)) {
        dwellStart = now;
        curChan = (curChan + 1) % MESH_CHANNEL_COUNT;
        lora_apply_channel(curChan);
        lora_start_listen();
        // Update footer only
#if HAS_OLED
        display_fill_rect_abs(0, 56, 128, 8, DISPLAY_BLACK);
#else
        display_fill_rect_abs(0, 122, 240, 13, DISPLAY_BLACK);
#endif
        char foot[32];
#if HAS_OLED
        snprintf(foot, sizeof(foot), "RX: %-10s", MESH_CHANNELS[curChan].name);
        display_draw_text_small_abs(0, 56, DISPLAY_YELLOW, foot);
#else
        snprintf(foot, sizeof(foot), "Listening: %-10s", MESH_CHANNELS[curChan].name);
        display_draw_text_small_abs(0, 126, DISPLAY_YELLOW, foot);
#endif
        // Re-draw table if OLED requires scroll update
#if HAS_OLED
        drawTable();
#endif
    }
}
