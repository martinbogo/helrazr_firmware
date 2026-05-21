/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "scanner.h"
#include "lora.h"
#include "display.h"
#include "button.h"

static const float   FREQ_START    = 902.0f;
static const float   FREQ_STEP     = 0.5f;
static const int     NUM_STEPS     = 53;
static const float   ACTIVE_THRESH = -100.0f;
static const int     MAX_KNOWN     = 30;       // max tracked channels
static const uint8_t STALE_AFTER   = 3;        // sweeps absent before marked stale
static const uint32_t SWEEP_MS     = 2000;

#if HAS_OLED
static const int ROWS_PER_PAGE = 3;
#else
static const int ROWS_PER_PAGE = 6;
#endif

struct KnownChannel {
    float   freq;
    float   lastRSSI;
    float   peakRSSI;
    uint8_t missCount;   // sweeps since last seen above threshold; 0 = active
};

static KnownChannel known[MAX_KNOWN];
static int      knownCount  = 0;
static int      scrollPage  = 0;
static int      totalPages  = 1;
static uint32_t lastSweep   = 0;
static int      sweepCount  = 0;

// -----------------------------------------------------------------------

static uint16_t rssiColor(float rssi) {
    if (rssi >= -80)  return DISPLAY_RED;
    if (rssi >= -100) return DISPLAY_YELLOW;
    return DISPLAY_GREEN;
}

static int rssiBarW(float rssi, int maxW) {
    float r = (rssi + 130.0f) / 90.0f;
    if (r < 0) r = 0; if (r > 1) r = 1;
    return (int)(r * maxW);
}

// -----------------------------------------------------------------------

static void drawPage() {
    int first = scrollPage * ROWS_PER_PAGE;
    int last  = first + ROWS_PER_PAGE;
    if (last > knownCount) last = knownCount;

#if HAS_OLED
    // Clear results area and footer
    display_fill_rect_abs(0, 18, 128, 46, DISPLAY_BLACK);

    if (knownCount == 0) {
        display_draw_text_small_abs(10, 34, DISPLAY_CYAN, "No channels found");
    } else {
        // Column header: "Freq   RSSI  Str" = 16 chars = 96px
        display_draw_text_small_abs(0,  18, DISPLAY_CYAN, "Freq   RSSI  Str");

        for (int i = first; i < last; i++) {
            int row = i - first;
            int y   = 27 + row * 9;   // y=27, 36, 45
            bool stale = (known[i].missCount >= STALE_AFTER);
            uint16_t col = stale ? DISPLAY_GRAY : rssiColor(known[i].lastRSSI);

            char freq[8], rssi[6];
            snprintf(freq, sizeof(freq), "%.2f", known[i].freq);
            snprintf(rssi, sizeof(rssi), "%4d", (int)known[i].lastRSSI);

            display_draw_text_small_abs(0,  y, col, freq);
            display_draw_text_small_abs(37, y, col, rssi);

            if (!stale) {
                int bw = rssiBarW(known[i].lastRSSI, 50);
                display_fill_rect_abs(73, y + 1, bw, 6, col);
                display_fill_rect_abs(73 + bw, y + 1, 50 - bw, 6, DISPLAY_BLACK);
            } else {
                display_draw_text_small_abs(73, y, DISPLAY_GRAY, "stale");
                display_fill_rect_abs(73 + 30, y + 1, 20, 6, DISPLAY_BLACK);
            }
        }
    }

    // Footer: "P1/2 N found  S:pg" or similar, max 21 chars
    char foot[32];
    if (totalPages > 1) {
        // "1/2  12 found  S:pg" = 19 chars = 114px ✓
        snprintf(foot, sizeof(foot), "%d/%d  %d found  S:pg",
                 scrollPage + 1, totalPages, knownCount);
    } else {
        // "12 found   sweeping" = 19 chars ✓
        snprintf(foot, sizeof(foot), "%d found   sweeping", knownCount);
    }
    display_draw_text_small_abs(0, 56, DISPLAY_CYAN, foot);

#else
    // TFT
    display_fill_rect_abs(0, 32, 240, 102, DISPLAY_BLACK);

    if (knownCount == 0) {
        display_draw_text_abs(50, 80, DISPLAY_CYAN, "No active channels");
    } else {
        display_draw_text_small_abs(0,   32, DISPLAY_CYAN, "Frequency");
        display_draw_text_small_abs(80,  32, DISPLAY_CYAN, "RSSI");
        display_draw_text_small_abs(120, 32, DISPLAY_CYAN, "Peak");
        display_draw_text_small_abs(162, 32, DISPLAY_CYAN, "Signal");

        for (int i = first; i < last; i++) {
            int row = i - first;
            int y   = 44 + row * 14;
            bool stale = (known[i].missCount >= STALE_AFTER);
            uint16_t col = stale ? DISPLAY_GRAY : rssiColor(known[i].lastRSSI);

            char freq[10], rssi[8], peak[8];
            snprintf(freq, sizeof(freq), "%.3f", known[i].freq);
            snprintf(rssi, sizeof(rssi), "%4d dBm", (int)known[i].lastRSSI);
            snprintf(peak, sizeof(peak), "%4d", (int)known[i].peakRSSI);

            display_draw_text_small_abs(0,   y, col, freq);
            display_draw_text_small_abs(80,  y, col, rssi);
            display_draw_text_small_abs(120, y, DISPLAY_GRAY, peak);

            if (!stale) {
                int bw = rssiBarW(known[i].lastRSSI, 74);
                display_fill_rect_abs(162, y, bw, 8, col);
                display_fill_rect_abs(162 + bw, y, 74 - bw, 8, DISPLAY_BLACK);
            } else {
                display_draw_text_small_abs(162, y, DISPLAY_GRAY, "stale");
            }
        }
    }

    char foot[48];
    if (totalPages > 1) {
        snprintf(foot, sizeof(foot), "Page %d/%d   %d channels   Short:next page",
                 scrollPage + 1, totalPages, knownCount);
    } else {
        snprintf(foot, sizeof(foot), "%d channels found   sweeping every 2s",
                 knownCount);
    }
    display_draw_text_small_abs(0, 126, DISPLAY_CYAN, foot);
#endif

    display_update_buffer();
}

// -----------------------------------------------------------------------

static void doSweep() {
    sweepCount++;

    // Age all known channels (reset to 0 when seen again below)
    for (int i = 0; i < knownCount; i++) {
        if (known[i].missCount < 255) known[i].missCount++;
    }

    // Scan band
    for (int step = 0; step < NUM_STEPS; step++) {
        float freq = FREQ_START + step * FREQ_STEP;
        float rssi = lora_scan_rssi(freq);
        if (rssi <= ACTIVE_THRESH) continue;

        // Update existing entry or add new
        bool found = false;
        for (int j = 0; j < knownCount; j++) {
            if (fabsf(known[j].freq - freq) < 0.1f) {
                known[j].lastRSSI = rssi;
                if (rssi > known[j].peakRSSI) known[j].peakRSSI = rssi;
                known[j].missCount = 0;
                found = true;
                break;
            }
        }
        if (!found && knownCount < MAX_KNOWN) {
            known[knownCount++] = { freq, rssi, rssi, 0 };
        }
    }

    // Sort: active channels first (missCount == 0), then stale; within each
    // group sort by lastRSSI descending (strongest first)
    for (int i = 1; i < knownCount; i++) {
        KnownChannel k = known[i];
        int j = i - 1;
        while (j >= 0) {
            bool cur_active  = (known[j].missCount == 0);
            bool key_active  = (k.missCount == 0);
            bool should_swap = false;
            if (!cur_active && key_active) {
                should_swap = true;   // active goes before stale
            } else if (cur_active == key_active) {
                should_swap = (known[j].lastRSSI < k.lastRSSI);  // stronger first
            }
            if (!should_swap) break;
            known[j + 1] = known[j];
            j--;
        }
        known[j + 1] = k;
    }

    // Recalculate pages; clamp scroll position
    totalPages = (knownCount + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
    if (totalPages < 1) totalPages = 1;
    if (scrollPage >= totalPages) scrollPage = 0;

    // Serial summary
    int activeCount = 0;
    for (int i = 0; i < knownCount; i++) if (known[i].missCount == 0) activeCount++;
    Serial.print("Scanner sweep "); Serial.print(sweepCount);
    Serial.print(": "); Serial.print(activeCount);
    Serial.print(" active, "); Serial.print(knownCount);
    Serial.println(" total:");
    for (int i = 0; i < knownCount; i++) {
        Serial.print("  "); Serial.print(known[i].freq, 3);
        Serial.print(" MHz  "); Serial.print((int)known[i].lastRSSI);
        Serial.print(" dBm");
        if (known[i].missCount > 0) {
            Serial.print("  (stale ");
            Serial.print(known[i].missCount);
            Serial.print(" sweeps)");
        }
        Serial.println();
    }
}

// -----------------------------------------------------------------------

void scanner_enter() {
    knownCount = 0;
    scrollPage = 0;
    totalPages = 1;
    lastSweep  = 0;
    sweepCount = 0;

    display_clear();
#if HAS_OLED
    display_draw_text_abs(10, 0, DISPLAY_CYAN, "Channel Scanner");
    display_draw_hline(0, 9, 128, DISPLAY_GRAY);
    display_draw_text_small_abs(0, 11, DISPLAY_CYAN, "Scanning 902-928MHz...");
#else
    display_draw_text_abs(40, 15, DISPLAY_CYAN, "Channel Scanner");
    display_draw_text_small_abs(0, 26, DISPLAY_CYAN, "Scanning 902-928 MHz...");
#endif
    display_update_buffer();
}

void scanner_short_press() {
    if (totalPages <= 1) return;
    scrollPage = (scrollPage + 1) % totalPages;
    drawPage();
}

void scanner_update() {
    uint32_t now = millis();
    if (now - lastSweep < SWEEP_MS) return;
    lastSweep = now;

    doSweep();
    drawPage();
}
