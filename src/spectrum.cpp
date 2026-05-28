/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "spectrum.h"
#include "lora.h"
#include "display.h"




static const float FREQ_START = 902.0f;
static const float FREQ_END   = 928.0f;
static const float FREQ_STEP  = 0.5f;
static const int   NUM_STEPS  = 53;

#if HAS_OLED
static const int GRAPH_X  = 0;
static const int GRAPH_Y  = 12;
static const int GRAPH_W  = 128;
static const int GRAPH_H  = 38;
#else
static const int GRAPH_X  = 0;
static const int GRAPH_Y  = 20;   // below title
static const int GRAPH_W  = 240;
static const int GRAPH_H  = 85;   // leaves ample room at bottom for 2 lines of labels
#endif
static const int RSSI_MIN = -135;
static const int RSSI_MAX = -40;

enum SpectrumMode {
    MODE_SWEEP = 0,
    MODE_HOLD,
    MODE_AVERAGE
};
static SpectrumMode currentMode = MODE_SWEEP;

static float rssiBuffer[53];
static float peakHoldBuffer[53];
static float avgBuffer[53];

static int rssiToHeight(float rssi) {
    if (rssi < RSSI_MIN) rssi = RSSI_MIN;
    if (rssi > RSSI_MAX) rssi = RSSI_MAX;
    return (int)((rssi - RSSI_MIN) / (float)(RSSI_MAX - RSSI_MIN) * GRAPH_H);
}

static uint16_t rssiToColor(float rssi) {
    if (rssi >= -80)  return DISPLAY_RED;
    if (rssi >= -100) return DISPLAY_YELLOW;
    return DISPLAY_GREEN;
}

void spectrum_short_press() {
    currentMode = (SpectrumMode)((currentMode + 1) % 3);
    if (currentMode == MODE_HOLD) {
        for (int i = 0; i < NUM_STEPS; i++) {
            peakHoldBuffer[i] = -200.0f;
        }
    } else if (currentMode == MODE_AVERAGE) {
        for (int i = 0; i < NUM_STEPS; i++) {
            avgBuffer[i] = rssiBuffer[i]; // seed with last sweep
        }
    }
}

void spectrum_enter() {
    currentMode = MODE_SWEEP;
    lora_set_scan_bandwidth(250.0f);
    display_clear();
#if HAS_OLED
    display_draw_text_abs(5, 0, DISPLAY_CYAN, "Spectrum 902-928 MHz");
    display_draw_text_small_abs(0,   GRAPH_Y + GRAPH_H + 4, DISPLAY_CYAN, "02");
    display_draw_text_small_abs(54,  GRAPH_Y + GRAPH_H + 4, DISPLAY_CYAN, "15");
    display_draw_text_small_abs(115, GRAPH_Y + GRAPH_H + 4, DISPLAY_CYAN, "28");
#else
    display_draw_text_abs(30, 15, DISPLAY_CYAN, "Spectrum 902-928 MHz");
    // x-axis labels using small font
    display_draw_text_small_abs(0,   GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "902");
    display_draw_text_small_abs(54,  GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "909");
    display_draw_text_small_abs(113, GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "916");
    display_draw_text_small_abs(172, GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "923");
    display_draw_text_small_abs(213, GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "928");
#endif
    display_update_buffer();
}

void spectrum_update() {
    float peakRSSI = -200, peakFreq = FREQ_START;

    for (int i = 0; i < NUM_STEPS; i++) {
        float freq = FREQ_START + i * FREQ_STEP;
        float rssi = lora_scan_rssi(freq);
        rssiBuffer[i] = rssi;
        
        if (currentMode == MODE_AVERAGE) {
            float alpha = 0.2f;
            if (avgBuffer[i] == -200.0f) avgBuffer[i] = rssi; // Initial seed guard
            avgBuffer[i] = (rssi * alpha) + (avgBuffer[i] * (1.0f - alpha));
            rssi = avgBuffer[i]; // Use smoothed value for drawing and peak calculation
        }

        if (rssi > peakRSSI) { peakRSSI = rssi; peakFreq = freq; }

        // Proportional bar placement fills the full graph width exactly
        int x0 = GRAPH_X + (i       * GRAPH_W) / NUM_STEPS;
        int x1 = GRAPH_X + ((i + 1) * GRAPH_W) / NUM_STEPS;
        int bw = x1 - x0;
        if (bw < 1) bw = 1;

        int h = rssiToHeight(rssi);
        display_fill_rect_abs(x0, GRAPH_Y,                bw, GRAPH_H - h, DISPLAY_BLACK);
        if (h > 0) display_fill_rect_abs(x0, GRAPH_Y + GRAPH_H - h, bw, h, rssiToColor(rssi));

        if (currentMode == MODE_HOLD) {
            if (rssi > peakHoldBuffer[i]) {
                peakHoldBuffer[i] = rssi;
            }
            int ph = rssiToHeight(peakHoldBuffer[i]);
            if (ph > 0) {
                int cx = x0 + bw / 2;
                int cy = GRAPH_Y + GRAPH_H - ph;
                if (i > 0) {
                    int prev_ph = rssiToHeight(peakHoldBuffer[i - 1]);
                    if (prev_ph > 0) {
                        int prev_x0 = GRAPH_X + ((i - 1) * GRAPH_W) / NUM_STEPS;
                        int prev_x1 = GRAPH_X + (i * GRAPH_W) / NUM_STEPS;
                        int prev_cx = prev_x0 + (prev_x1 - prev_x0) / 2;
                        int prev_cy = GRAPH_Y + GRAPH_H - prev_ph;
                        display_draw_line(prev_cx, prev_cy, cx, cy, DISPLAY_CYAN);
                    } else {
                        display_draw_hline(x0, cy, bw, DISPLAY_CYAN);
                    }
                } else {
                    display_draw_hline(x0, cy, bw, DISPLAY_CYAN);
                }
            }
        }
    }

    // End of sweep: update peak annotation
    char peak[36];
    const char* modeStr = "[S] ";
    if (currentMode == MODE_HOLD) modeStr = "[H] ";
    else if (currentMode == MODE_AVERAGE) modeStr = "[A] ";

#if HAS_OLED
    snprintf(peak, sizeof(peak), "%sPk:%.1fMHz %ddBm", modeStr, peakFreq, (int)peakRSSI);
    display_fill_rect_abs(0, GRAPH_Y + GRAPH_H + 2, GRAPH_W, 10, DISPLAY_BLACK);
    display_draw_text_small_abs(0, GRAPH_Y + GRAPH_H + 4, DISPLAY_WHITE, peak);
#else
    snprintf(peak, sizeof(peak), "%sPeak: %.1fMHz  %ddBm", modeStr, peakFreq, (int)peakRSSI);
    display_draw_text_line(0, GRAPH_Y + GRAPH_H + 28, DISPLAY_WHITE, peak);
#endif

    display_update_buffer();
}
