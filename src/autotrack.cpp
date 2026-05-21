/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "autotrack.h"
#include "lora.h"
#include "display.h"
#include "nodetracker.h"
#include "stats.h"




static const float FREQ_START   = 902.0f;
static const float FREQ_STEP    = 0.5f;
static const int   NUM_STEPS    = 53;
static const uint32_t RESCAN_MS = 30000;  // rescan every 30s

static float    lockedFreq  = 906.875f;
static float    lockedRSSI  = -200.0f;
static int      totalPkts   = 0;
static uint32_t lastRescanMs = 0;

static void doScan() {
    float bestRSSI = -200, bestFreq = FREQ_START;
    for (int i = 0; i < NUM_STEPS; i++) {
        float f = FREQ_START + i * FREQ_STEP;
        float r = lora_scan_rssi(f);
        if (r > bestRSSI) { bestRSSI = r; bestFreq = f; }
    }
    lockedFreq = bestFreq;
    lockedRSSI = bestRSSI;
    lora_set_frequency(lockedFreq);
    lora_set_bandwidth(250.0f);
    lora_set_spreading_factor(11);
    lora_start_listen();
    lastRescanMs = millis();

    Serial.print("AutoTrack: locked to ");
    Serial.print(lockedFreq, 3);
    Serial.print(" MHz  RSSI=");
    Serial.println((int)lockedRSSI);
}

static void drawScreen() {
    display_clear();
#if HAS_OLED
    display_draw_text_abs(15, 0, DISPLAY_CYAN, "Auto Freq Tracker");
    display_draw_hline(0, 10, 128, DISPLAY_GRAY);
    char buf[40];
    snprintf(buf, sizeof(buf), "Freq: %.2f MHz", lockedFreq);
    display_draw_text_small_abs(0, 15, DISPLAY_GREEN, buf);
    snprintf(buf, sizeof(buf), "RSSI: %ddBm", (int)lockedRSSI);
    display_draw_text_small_abs(0, 25, DISPLAY_YELLOW, buf);
    snprintf(buf, sizeof(buf), "Packets: %d", totalPkts);
    display_draw_text_small_abs(0, 35, DISPLAY_WHITE, buf);
    uint32_t remaining = RESCAN_MS - min((uint32_t)(millis() - lastRescanMs),  (uint32_t)RESCAN_MS);
    snprintf(buf, sizeof(buf), "Rescan: %lus", remaining / 1000);
    display_draw_text_small_abs(0, 45, DISPLAY_CYAN, buf);
    snprintf(buf, sizeof(buf), "Nodes: %d", nodetracker_count());
    display_draw_text_small_abs(0, 55, DISPLAY_CYAN, buf);
#else
    display_draw_text_abs(35, 15, DISPLAY_CYAN, "Auto Freq Tracker");
    display_draw_hline(0, 20, 240, DISPLAY_GRAY);
    char buf[40];
    snprintf(buf, sizeof(buf), "Freq: %.3f MHz", lockedFreq);
    display_draw_text_abs(0, 42, DISPLAY_GREEN, buf);
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)lockedRSSI);
    display_draw_text_abs(0, 60, DISPLAY_YELLOW, buf);
    snprintf(buf, sizeof(buf), "Packets: %d", totalPkts);
    display_draw_text_abs(0, 78, DISPLAY_WHITE, buf);
    uint32_t remaining = RESCAN_MS - min((uint32_t)(millis() - lastRescanMs),  (uint32_t)RESCAN_MS);
    snprintf(buf, sizeof(buf), "Rescan in: %lus", remaining / 1000);
    display_draw_text_abs(0, 96, DISPLAY_CYAN, buf);
    int nodeCount = nodetracker_count();
    snprintf(buf, sizeof(buf), "Nodes heard: %d", nodeCount);
    display_draw_text_abs(0, 114, DISPLAY_CYAN, buf);
#endif
}

void autotrack_enter() {
    totalPkts = 0; lastRescanMs = 0;
    display_clear();
#if HAS_OLED
    display_draw_text_abs(15, 0, DISPLAY_CYAN, "Auto Freq Tracker");
    display_draw_text_small_abs(20, 30, DISPLAY_CYAN, "Scanning band...");
#else
    display_draw_text_abs(35, 15, DISPLAY_CYAN, "Auto Freq Tracker");
    display_draw_text_abs(30, 70, DISPLAY_CYAN, "Scanning band...");
#endif
    doScan();
    drawScreen();
}

void autotrack_update() {
    if (lora_poll_packet()) {
        totalPkts++;
        uint8_t buf[256]; int len = 0;
        lora_get_last_packet(buf, &len);
        // Parse Meshtastic header if long enough
        if (len >= 8) {
            uint32_t srcNode;
            memcpy(&srcNode, buf + 4, 4);
            nodetracker_update(srcNode, lora_last_rssi(), lora_last_snr());
            stats_record_packet(srcNode, lora_last_rssi());
        }
        drawScreen();
        Serial.print("AutoTrack RX: RSSI=");
        Serial.print(lora_last_rssi());
        Serial.println(" dBm");
    }

    // Periodic rescan
    if (millis() - lastRescanMs >= RESCAN_MS) {
#if HAS_OLED
        display_fill_rect_abs(0, 45, 128, 8, DISPLAY_BLACK);
        display_draw_text_small_abs(0, 45, DISPLAY_CYAN, "Rescanning...");
#else
        display_fill_rect_abs(0, 88, 240, 16, DISPLAY_BLACK);
        display_draw_text_abs(30, 100, DISPLAY_CYAN, "Rescanning...");
#endif
        doScan();
        drawScreen();
    }

    // Update countdown every second without full redraw
    static uint32_t lastCountdown = 0;
    if (millis() - lastCountdown > 1000) {
        lastCountdown = millis();
        uint32_t remaining = RESCAN_MS - min((uint32_t)(millis() - lastRescanMs),  (uint32_t)RESCAN_MS);
        char buf[32];
#if HAS_OLED
        snprintf(buf, sizeof(buf), "Rescan: %lus  ", remaining / 1000);
        display_fill_rect_abs(0, 45, 128, 8, DISPLAY_BLACK);
        display_draw_text_small_abs(0, 45, DISPLAY_CYAN, buf);
#else
        snprintf(buf, sizeof(buf), "Rescan in: %lus  ", remaining / 1000);
        display_fill_rect_abs(0, 86, 200, 16, DISPLAY_BLACK);
        display_draw_text_abs(0, 96, DISPLAY_CYAN, buf);
#endif
    }
}
