/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "waterfall.h"
#include "lora.h"
#include "display.h"
#include <string.h>

static const float FREQ_START = 902.0f;
static const float FREQ_END   = 928.0f;
static const float FREQ_STEP  = 0.5f;
static const int   NUM_STEPS  = 53;

#if HAS_OLED
static const int GRAPH_X  = 0;
static const int GRAPH_Y  = 12;
static const int GRAPH_W  = 128;
static const int GRAPH_H  = 40;
#else
static const int GRAPH_X  = 0;
static const int GRAPH_Y  = 20;
static const int GRAPH_W  = 240;
static const int GRAPH_H  = 90;
#endif

// We map history to an array of size GRAPH_H * NUM_STEPS
// Each cell holds RSSI offset (rssi - -135) to fit in uint8_t
static uint8_t history[GRAPH_H][NUM_STEPS];
static int nextRow = 0;
static uint32_t totalScans = 0;
static int crosshairRank = -1;
static float noiseFloor = -120.0f;

#if !HAS_OLED
static int colorMode = 0; // 0=Classic, 1=Grayscale, 2=Ironbow, 3=Viridis, 4=Ocean, 5=Matrix, 6=Magma, 7=Super Bright

// Helper for RGB565 encoding
#ifndef C565
#define C565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#endif

static const uint16_t PALLETES[8][8] = {
    // 0: Classic
    { 0x0000, 0x0017, 0x001F, 0x07FF, 0x07E0, 0xFFE0, 0xFD20, 0xF800 },
    // 1: Grayscale
    { 0x0000, C565(36,36,36), C565(73,73,73), C565(109,109,109), C565(146,146,146), C565(182,182,182), C565(219,219,219), C565(255,255,255) },
    // 2: Ironbow
    { 0x0000, C565(20,0,40), C565(80,0,80), C565(150,0,0), C565(255,100,0), C565(255,180,0), C565(255,255,0), C565(255,255,255) },
    // 3: Viridis
    { 0x0000, C565(68,1,84), C565(72,40,120), C565(49,104,142), C565(38,130,142), C565(53,183,121), C565(143,215,68), C565(253,231,37) },
    // 4: Ocean
    { 0x0000, C565(0,0,40), C565(0,0,80), C565(0,50,150), C565(0,100,200), C565(0,150,255), C565(0,255,255), C565(255,255,255) },
    // 5: Matrix Green
    { 0x0000, C565(0,20,0), C565(0,50,0), C565(0,90,0), C565(0,140,0), C565(0,200,0), C565(0,255,0), C565(150,255,150) },
    // 6: Magma
    { 0x0000, C565(20,10,30), C565(50,10,60), C565(100,10,80), C565(180,30,80), C565(230,80,90), C565(250,150,100), C565(255,255,255) },
    // 7: Super Bright
    { 0x0000, C565(0,50,255), C565(255,0,0), C565(255,128,0), C565(255,255,0), C565(255,255,128), C565(255,255,255), C565(255,255,255) }
};

static void drawTftColorLegend() {
    // Draw 8 blocks of 16 pixels wide by 8 pixels high centered at the very bottom (y=126)
    // Display height is 135. Frequencies are drawn at y=115..123.
    // Width = 8 * 16 = 128. Start x = (240 - 128) / 2 = 56
    for (int i = 0; i < 8; i++) {
        display_fill_rect_abs(56 + i * 16, 126, 16, 8, PALLETES[colorMode][i]);
    }
}
#else
static bool invertOled = false;
#endif

#if HAS_OLED
static int getOledLevel(int rssi) {
    float above = rssi - noiseFloor;
    if (above < 1.5f) return 0;
    if (above < 5)    return 1;
    if (above < 12)   return 2;
    if (above < 22)   return 3;
    return 4;
}
#endif

// Helper for RGB565 encoding
#ifndef C565
#define C565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#endif

static uint16_t rssiToWaterfallColor(int rssi) {
#if HAS_OLED
    return DISPLAY_BLACK;
#else
    float above = rssi - noiseFloor;
    int level = 0;
    if      (above >= 40) level = 7;
    else if (above >= 32) level = 6;
    else if (above >= 24) level = 5;
    else if (above >= 18) level = 4;
    else if (above >= 12) level = 3;
    else if (above >= 6)  level = 2;
    else if (above >= 3)  level = 1;

    return PALLETES[colorMode][level];
#endif
}

void waterfall_short_press() {
    crosshairRank++;
    if (crosshairRank >= 3) { // Cycle through top 3 peaks, then off
        crosshairRank = -1;
    }

#if HAS_OLED
    // Clear old pause UI area if it was left dirty (not needed anymore, but safe)
    display_fill_rect_abs(112, 0, 16, 10, DISPLAY_BLACK);
    display_draw_text_abs(5, 0, DISPLAY_CYAN, "Waterfall 902-928MHz");
#else
    display_fill_rect_abs(220, 5, 20, 20, DISPLAY_BLACK);
#endif
    display_update_buffer();
}

void waterfall_double_press() {
#if !HAS_OLED
    colorMode = (colorMode + 1) % 8;
    drawTftColorLegend();
#else
    invertOled = !invertOled;
#endif
}

void waterfall_enter() {
    memset(history, 0, sizeof(history));
    nextRow = 0;
    totalScans = 0;
    crosshairRank = -1;
    noiseFloor = -120.0f;

    lora_set_scan_bandwidth(250.0f);
    display_clear();
#if HAS_OLED
    display_draw_text_abs(5, 0, DISPLAY_CYAN, "Waterfall 902-928MHz");
    display_draw_text_tiny_abs(0,   GRAPH_Y + GRAPH_H + 3, DISPLAY_CYAN, "902");
    display_draw_text_tiny_abs(54,  GRAPH_Y + GRAPH_H + 3, DISPLAY_CYAN, "915");
    display_draw_text_tiny_abs(111, GRAPH_Y + GRAPH_H + 3, DISPLAY_CYAN, "928");
#else
    display_draw_text_abs(30, 15, DISPLAY_CYAN, "Waterfall 902-928 MHz");
    drawTftColorLegend();
    display_draw_text_small_abs(0,   GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "902");
    display_draw_text_small_abs(54,  GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "909");
    display_draw_text_small_abs(113, GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "916");
    display_draw_text_small_abs(172, GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "923");
    display_draw_text_small_abs(213, GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "928");
#endif
    display_update_buffer();
}

void waterfall_update() {
    // Capture one row of data
    float rowRssi[NUM_STEPS];
    for (int i = 0; i < NUM_STEPS; i++) {
        float freq = FREQ_START + i * FREQ_STEP;
        float rssi = lora_scan_rssi(freq);
        rowRssi[i] = rssi;

        int val = (int)rssi + 135;
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        history[nextRow][i] = val;
    }

    // Adaptive noise floor: find the median RSSI and smooth it
    float sorted[NUM_STEPS];
    memcpy(sorted, rowRssi, sizeof(sorted));
    for (int i = 1; i < NUM_STEPS; i++) {
        float k = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > k) { sorted[j+1] = sorted[j]; j--; }
        sorted[j+1] = k;
    }
    float sweepFloor = sorted[NUM_STEPS / 4];
    noiseFloor = 0.2f * sweepFloor + 0.8f * noiseFloor;

    nextRow = (nextRow + 1) % GRAPH_H;
    totalScans++;

#if !HAS_OLED
    // High-speed SPI burst renderer for TFT
    uint16_t rowBuf[240]; 
    for (int r = 0; r < GRAPH_H; r++) {
        int histRow = (nextRow - 1 - r + GRAPH_H) % GRAPH_H;
        int y = GRAPH_Y + r;

        for (int i = 0; i < NUM_STEPS; i++) {
            int x0 = (i       * GRAPH_W) / NUM_STEPS;
            int x1 = ((i + 1) * GRAPH_W) / NUM_STEPS;
            int rssi = history[histRow][i] - 135;
            uint16_t color = rssiToWaterfallColor(rssi);
            
            for (int px = x0; px < x1 && px < GRAPH_W; px++) {
                rowBuf[px] = color;
            }
        }
        tft.drawRGBBitmap(GRAPH_X, y, rowBuf, GRAPH_W, 1);
    }
#else
    // Draw the entire buffer
    for (int r = 0; r < GRAPH_H; r++) {
        // Map logical row (newest at bottom or top?)
        // Let's make newest row at the top (r=0), oldest at bottom (r=GRAPH_H-1)
        int histRow = (nextRow - 1 - r + GRAPH_H) % GRAPH_H;
        uint32_t scanIndex = totalScans - 1 - r;
        
        int y = GRAPH_Y + r;
        
        for (int i = 0; i < NUM_STEPS; i++) {
            int x0 = GRAPH_X + (i       * GRAPH_W) / NUM_STEPS;
            int x1 = GRAPH_X + ((i + 1) * GRAPH_W) / NUM_STEPS;
            int bw = x1 - x0;
            if (bw < 1) bw = 1;
            
            int rssi = history[histRow][i] - 135;

#if HAS_OLED
            int level = getOledLevel(rssi);
            
            if (invertOled) {
                // Inverted mode introduces OLED blooming (light bleed) if we dither.
                // To guarantee signals look "fully black", we draw noise as solid white, 
                // and ANY level of signal as a pure black silhouette.
                if (level == 0) {
                    display_fill_rect_abs(x0, y, bw, 1, DISPLAY_WHITE);
                } else {
                    display_fill_rect_abs(x0, y, bw, 1, DISPLAY_BLACK);
                }
            } else {
                if (level == 0) {
                    display_fill_rect_abs(x0, y, bw, 1, DISPLAY_BLACK);
                } else if (level == 4) {
                    display_fill_rect_abs(x0, y, bw, 1, DISPLAY_WHITE);
                } else {
                    // 2x2 Bayer ordered dither matrix (perfectly maps to our 5 OLED levels)
                    static const uint8_t bayer2x2[2][2] = {
                        { 0, 2 },
                        { 3, 1 }
                    };
                    for (int px = x0; px < x1; px++) {
                        bool fill_white = (bayer2x2[scanIndex % 2][px % 2] < level);
                        
                        if (fill_white) {
                            display_fill_rect_abs(px, y, 1, 1, DISPLAY_WHITE);
                        } else {
                            display_fill_rect_abs(px, y, 1, 1, DISPLAY_BLACK);
                        }
                    }
                }
            }
#else
            uint16_t color = rssiToWaterfallColor(rssi);
            display_fill_rect_abs(x0, y, bw, 1, color);
#endif
        }
    }
#endif

    if (crosshairRank >= 0) {
        int colSums[NUM_STEPS] = {0};
        long totalSum = 0;
        for (int r = 0; r < GRAPH_H; r++) {
            for (int i = 0; i < NUM_STEPS; i++) {
                colSums[i] += history[r][i];
                totalSum += history[r][i];
            }
        }
        
        // Threshold: use adaptive noise floor instead of fixed offset
        int noiseSum = (int)(noiseFloor + 135) * GRAPH_H;
        int threshold = noiseSum + GRAPH_H * 2;

        struct Peak { int col; int sum; };
        Peak peaks[NUM_STEPS];
        int numPeaks = 0;

        for (int i = 0; i < NUM_STEPS; i++) {
            if (colSums[i] <= threshold) continue;
            // Accept if higher than both neighbors, or at band edges
            bool isPeak = (i == 0 || colSums[i] >= colSums[i-1]) &&
                          (i == NUM_STEPS-1 || colSums[i] >= colSums[i+1]);
            if (isPeak) {
                peaks[numPeaks].col = i;
                peaks[numPeaks].sum = colSums[i];
                numPeaks++;
            }
        }
        
        // Sort descending by energy sum
        for (int i = 1; i < numPeaks; i++) {
            Peak k = peaks[i];
            int j = i - 1;
            while (j >= 0 && peaks[j].sum < k.sum) { peaks[j+1] = peaks[j]; j--; }
            peaks[j+1] = k;
        }

        if (numPeaks > 0 && crosshairRank < numPeaks) {
            int targetCol = peaks[crosshairRank].col;
            float freq = FREQ_START + targetCol * FREQ_STEP;
            
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", freq);

#if HAS_OLED
            int x0 = GRAPH_X + (targetCol       * GRAPH_W) / NUM_STEPS;
            int x1 = GRAPH_X + ((targetCol + 1) * GRAPH_W) / NUM_STEPS;
            int cx = (x0 + x1) / 2;

            display_fill_rect_abs(cx, GRAPH_Y, 1, GRAPH_H, invertOled ? DISPLAY_BLACK : DISPLAY_WHITE);

            // Show frequency in title area instead of overlaying on the graph
            char title[22];
            snprintf(title, sizeof(title), "Waterfall  %s MHz", buf);
            display_fill_rect_abs(0, 0, 128, 10, DISPLAY_BLACK);
            display_draw_text_small_abs(5, 0, DISPLAY_CYAN, title);
#else
            // High-speed SPI burst draws rows directly on TFT, so we must calculate cx for the whole screen
            int cx = (targetCol * GRAPH_W) / NUM_STEPS + GRAPH_X + (GRAPH_W / NUM_STEPS / 2);

            display_fill_rect_abs(cx, GRAPH_Y, 1, GRAPH_H, 0xF81F); // Magenta line
            int boxX = cx + 2;
            if (boxX + 48 > 240) boxX = cx - 50; 
            display_fill_rect_abs(boxX, GRAPH_Y + 5, 48, 14, DISPLAY_BLACK);
            display_draw_text_small_abs(boxX + 4, GRAPH_Y + 16, 0xF81F, buf);
#endif
        } else {
            // Rank exceeds available peaks, or no peaks found
            crosshairRank = -1;
        }
    }

    display_update_buffer();
}
