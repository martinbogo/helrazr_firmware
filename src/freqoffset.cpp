/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "freqoffset.h"
#include "lora.h"
#include "display.h"
#include "modes.h"

static const uint32_t DWELL_MS = 5000;

struct ChannelOffset {
    float offsetHz;
    float rssi;
    int   packets;
    bool  valid;
};

static ChannelOffset offsets[MESH_CHANNEL_COUNT];
static int      curChan    = 0;
static uint32_t dwellStart = 0;
static bool     showVisual = false;

static uint16_t offsetColor(float absHz) {
    if (absHz > 15000.0f) return 0xFD20;
    if (absHz > 5000.0f)  return DISPLAY_YELLOW;
    return DISPLAY_GREEN;
}

static void drawDisplay() {
#if HAS_OLED
    display_clear();
    display_draw_text_abs(10, 0, DISPLAY_CYAN, "Freq Offset");
    display_draw_hline(0, 10, 128, DISPLAY_GRAY);

    for (int i = 0; i < MESH_CHANNEL_COUNT; i++) {
        int y = 12 + i * 7;
        uint16_t col = (i == curChan) ? DISPLAY_YELLOW : DISPLAY_WHITE;

        display_draw_text_small_abs(0, y, col, MESH_CHANNELS[i].name);

        if (offsets[i].valid) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%+.1fkHz %ddBm",
                     offsets[i].offsetHz / 1000.0f, (int)offsets[i].rssi);
            display_draw_text_small_abs(50, y, offsetColor(fabsf(offsets[i].offsetHz)), buf);
        } else {
            display_draw_text_small_abs(50, y, DISPLAY_GRAY, "waiting...");
        }
    }

    char foot[24];
    snprintf(foot, sizeof(foot), "RX: %-10s", MESH_CHANNELS[curChan].name);
    display_draw_text_small_abs(0, 56, DISPLAY_YELLOW, foot);
#else
    display_draw_text_line(30, 15, DISPLAY_CYAN, "Frequency Offset");
    display_draw_hline(0, 20, 240, DISPLAY_GRAY);

    if (showVisual) {
        display_draw_vline(120, 24, 98, DISPLAY_GRAY);

        for (int i = 0; i < MESH_CHANNEL_COUNT; i++) {
            int y = 36 + i * 14;
            uint16_t col = (i == curChan) ? DISPLAY_YELLOW : DISPLAY_WHITE;

            display_begin_line(y, false);
            display_line_text(0, col, MESH_CHANNELS[i].name);

            if (offsets[i].valid) {
                float maxHz = 20000.0f;
                int pixels = (int)(offsets[i].offsetHz / maxHz * 50.0f);
                if (pixels > 50) pixels = 50;
                if (pixels < -50) pixels = -50;

                uint16_t barCol = offsetColor(fabsf(offsets[i].offsetHz));
                if (pixels > 0) {
                    display_line_fill_rect(120, pixels, barCol);
                } else if (pixels < 0) {
                    display_line_fill_rect(120 + pixels, -pixels, barCol);
                }

                char lbl[12];
                snprintf(lbl, sizeof(lbl), "%+.1fk", offsets[i].offsetHz / 1000.0f);
                display_line_text(180, barCol, lbl);
            } else {
                display_line_text(120, DISPLAY_GRAY, "--");
            }
            display_end_line();
        }
    } else {
        for (int i = 0; i < MESH_CHANNEL_COUNT; i++) {
            int y = 36 + i * 14;
            uint16_t col = (i == curChan) ? DISPLAY_YELLOW : DISPLAY_WHITE;

            display_begin_line(y, false);
            display_line_text(0, col, MESH_CHANNELS[i].name);

            if (offsets[i].valid) {
                char off[16], rssi[10];
                snprintf(off, sizeof(off), "%+.1f kHz", offsets[i].offsetHz / 1000.0f);
                snprintf(rssi, sizeof(rssi), "%d", (int)offsets[i].rssi);
                display_line_text(95, offsetColor(fabsf(offsets[i].offsetHz)), off);
                display_line_text(195, DISPLAY_WHITE, rssi);
            } else {
                display_line_text(95, DISPLAY_GRAY, "waiting...");
            }
            display_end_line();
        }
    }

    char foot[40];
    snprintf(foot, sizeof(foot), "Listening: %-10s", MESH_CHANNELS[curChan].name);
    display_draw_text_small_line(0, 133, DISPLAY_YELLOW, foot);
#endif
    display_update_buffer();
}

void freqoffset_enter() {
    for (int i = 0; i < MESH_CHANNEL_COUNT; i++) {
        offsets[i].valid = false;
        offsets[i].packets = 0;
    }
    curChan = 0;
    dwellStart = millis();
    showVisual = false;
    lora_apply_channel(0);
    lora_start_listen();
    drawDisplay();
}

void freqoffset_update() {
    if (lora_poll_packet()) {
        float freqErr = lora_last_freq_error();
        float rssi = lora_last_rssi();

        offsets[curChan].packets++;
        if (!offsets[curChan].valid) {
            offsets[curChan].offsetHz = freqErr;
            offsets[curChan].rssi = rssi;
        } else {
            // EMA to smooth across multiple packets
            offsets[curChan].offsetHz = 0.3f * freqErr + 0.7f * offsets[curChan].offsetHz;
            offsets[curChan].rssi = rssi;
        }
        offsets[curChan].valid = true;

        Serial.print("FreqOffset [");
        Serial.print(MESH_CHANNELS[curChan].name);
        Serial.print("] err=");
        Serial.print(freqErr, 1);
        Serial.print("Hz RSSI=");
        Serial.println(rssi);

        drawDisplay();
    }

    uint32_t now = millis();
    if (now - dwellStart >= DWELL_MS) {
        dwellStart = now;
        curChan = (curChan + 1) % MESH_CHANNEL_COUNT;
        lora_apply_channel(curChan);
        lora_start_listen();
        drawDisplay();
    }
}

void freqoffset_short_press() {
    showVisual = !showVisual;
    drawDisplay();
}
