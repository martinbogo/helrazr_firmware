/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "duty.h"
#include "lora.h"
#include "display.h"
#include "modes.h"

static const uint32_t DWELL_MS         = 2000;
static const uint32_t SAMPLE_INTERVAL  = 50;
static const float    ACTIVITY_THRESH  = -110.0f;
static const float    EMA_ALPHA        = 0.3f;

static float    dutyCycle[MESH_CHANNEL_COUNT];
static int      curChan       = 0;
static uint32_t dwellStart    = 0;
static uint32_t lastSample    = 0;
static int      samplesAbove  = 0;
static int      samplesTotal  = 0;
static bool     showBars      = true;

static uint16_t dutyColor(float pct) {
    if (pct > 60.0f) return DISPLAY_RED;
    if (pct > 30.0f) return DISPLAY_YELLOW;
    return DISPLAY_GREEN;
}

static void drawDisplay() {
#if HAS_OLED
    display_clear();
    display_draw_text_abs(15, 0, DISPLAY_CYAN, "Duty Cycle");
    display_draw_hline(0, 10, 128, DISPLAY_GRAY);

    for (int i = 0; i < MESH_CHANNEL_COUNT; i++) {
        int y = 12 + i * 7;
        uint16_t col = (i == curChan) ? DISPLAY_YELLOW : DISPLAY_WHITE;
        char name[10];
        snprintf(name, sizeof(name), "%-8s", MESH_CHANNELS[i].name);
        display_draw_text_small_abs(0, y, col, name);

        int barW = (int)(dutyCycle[i] / 100.0f * 60);
        if (barW > 60) barW = 60;
        display_fill_rect_abs(50, y + 1, barW, 5, dutyColor(dutyCycle[i]));
        display_fill_rect_abs(50 + barW, y + 1, 60 - barW, 5, DISPLAY_BLACK);

        char pct[8];
        snprintf(pct, sizeof(pct), "%3d%%", (int)dutyCycle[i]);
        display_draw_text_small_abs(112, y, col, pct);
    }

    char foot[24];
    snprintf(foot, sizeof(foot), "RX: %-10s", MESH_CHANNELS[curChan].name);
    display_draw_text_small_abs(0, 56, DISPLAY_YELLOW, foot);
#else
    display_draw_text_line(20, 15, DISPLAY_CYAN, "Channel Utilization");
    display_draw_hline(0, 20, 240, DISPLAY_GRAY);

    for (int i = 0; i < MESH_CHANNEL_COUNT; i++) {
        int y = 36 + i * 14;
        uint16_t col = (i == curChan) ? DISPLAY_YELLOW : DISPLAY_WHITE;

        if (showBars) {
            display_begin_line(y, false);
            display_line_text(0, col, MESH_CHANNELS[i].name);

            int barW = (int)(dutyCycle[i] / 100.0f * 90);
            if (barW > 90) barW = 90;
            display_line_fill_rect(100, barW, dutyColor(dutyCycle[i]));
            display_line_fill_rect(100 + barW, 90 - barW, DISPLAY_BLACK);

            char pct[8];
            snprintf(pct, sizeof(pct), "%d%%", (int)dutyCycle[i]);
            display_line_text(195, col, pct);
            display_end_line();
        } else {
            char label[16];
            if (dutyCycle[i] < 5.0f)       snprintf(label, sizeof(label), "Quiet");
            else if (dutyCycle[i] < 30.0f)  snprintf(label, sizeof(label), "Low");
            else if (dutyCycle[i] < 60.0f)  snprintf(label, sizeof(label), "Moderate");
            else                            snprintf(label, sizeof(label), "Busy");

            char pct[8];
            snprintf(pct, sizeof(pct), "%d%%", (int)dutyCycle[i]);

            display_begin_line(y, false);
            display_line_text(0, col, MESH_CHANNELS[i].name);
            display_line_text(105, col, pct);
            display_line_text(155, dutyColor(dutyCycle[i]), label);
            display_end_line();
        }
    }

    char foot[40];
    snprintf(foot, sizeof(foot), "Scanning: %-10s", MESH_CHANNELS[curChan].name);
    display_draw_text_small_line(0, 133, DISPLAY_YELLOW, foot);
#endif
    display_update_buffer();
}

void duty_enter() {
    for (int i = 0; i < MESH_CHANNEL_COUNT; i++) dutyCycle[i] = 0.0f;
    curChan = 0;
    samplesAbove = 0;
    samplesTotal = 0;
    showBars = true;
    dwellStart = millis();
    lastSample = millis();
    lora_apply_channel(0);
    drawDisplay();
}

void duty_update() {
    uint32_t now = millis();

    if (now - lastSample >= SAMPLE_INTERVAL) {
        lastSample = now;
        float rssi = lora_scan_rssi(MESH_CHANNELS[curChan].freqMHz);
        samplesTotal++;
        if (rssi > ACTIVITY_THRESH) samplesAbove++;
    }

    if (now - dwellStart >= DWELL_MS && samplesTotal > 0) {
        float raw = 100.0f * samplesAbove / samplesTotal;
        dutyCycle[curChan] = EMA_ALPHA * raw + (1.0f - EMA_ALPHA) * dutyCycle[curChan];

        curChan = (curChan + 1) % MESH_CHANNEL_COUNT;
        samplesAbove = 0;
        samplesTotal = 0;
        dwellStart = now;
        lora_apply_channel(curChan);
        drawDisplay();
    }
}

void duty_short_press() {
    showBars = !showBars;
    drawDisplay();
}
