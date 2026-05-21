#include "noise.h"
#include "lora.h"
#include "display.h"
#include "modes.h" // For MESH_CHANNELS

#if HAS_OLED
static const int GRAPH_X  = 0;
static const int GRAPH_Y  = 12;
static const int GRAPH_W  = 128;
static const int GRAPH_H  = 38;
#else
static const int GRAPH_X  = 0;
static const int GRAPH_Y  = 20;
static const int GRAPH_W  = 240;
static const int GRAPH_H  = 85;
#endif

static const int RSSI_MIN = -135;
static const int RSSI_MAX = -40;

static float history[240]; 
static int historyIdx = 0;
static int channelIdx = 0;
static uint32_t lastSample = 0;

static int rssiToHeight(float rssi) {
    if (rssi < RSSI_MIN) rssi = RSSI_MIN;
    if (rssi > RSSI_MAX) rssi = RSSI_MAX;
    return (int)((rssi - RSSI_MIN) / (float)(RSSI_MAX - RSSI_MIN) * GRAPH_H);
}

void noise_short_press() {
    channelIdx = (channelIdx + 1) % MESH_CHANNEL_COUNT;
    noise_enter();
}

void noise_enter() {
    for (int i = 0; i < GRAPH_W; i++) history[i] = -140;
    historyIdx = 0;
    lora_apply_channel(channelIdx);
    lora_start_listen();
    
    display_clear();
#if HAS_OLED
    display_draw_text_abs(5, 0, DISPLAY_CYAN, MESH_CHANNELS[channelIdx].name);
#else
    char buf[40];
    snprintf(buf, sizeof(buf), "Noise Floor: %s (%.1f)", MESH_CHANNELS[channelIdx].name, MESH_CHANNELS[channelIdx].freqMHz);
    display_draw_text_abs(5, 15, DISPLAY_CYAN, buf);
#endif
    display_update_buffer();
}

void noise_update() {
    if (millis() - lastSample < 50) return; // Sample every 50ms
    lastSample = millis();

    float rssi = lora_scan_rssi(MESH_CHANNELS[channelIdx].freqMHz);
    
    // Shift history
    for (int i = 0; i < GRAPH_W - 1; i++) {
        history[i] = history[i+1];
    }
    history[GRAPH_W - 1] = rssi;

    // Draw graph - use vlines to overwrite instead of fill_rect to prevent flickering
    for (int i = 0; i < GRAPH_W; i++) {
        int h = 0;
        if (history[i] > RSSI_MIN) {
            h = rssiToHeight(history[i]);
        }
        
        // Draw black space above the bar
        if (GRAPH_H - h > 0) {
            display_draw_vline(GRAPH_X + i, GRAPH_Y, GRAPH_H - h, DISPLAY_BLACK);
        }
        
        if (h > 0) {
            // Determine color based on threshold
            uint16_t color = DISPLAY_GREEN;
            if (history[i] > -90) color = DISPLAY_RED;
            else if (history[i] > -110) color = DISPLAY_YELLOW;
            display_draw_vline(GRAPH_X + i, GRAPH_Y + GRAPH_H - h, h, color);
        }
    }

    char stat[32];
#if HAS_OLED
    snprintf(stat, sizeof(stat), "Cur: %d dBm", (int)rssi);
    display_fill_rect_abs(0, GRAPH_Y + GRAPH_H + 2, GRAPH_W, 10, DISPLAY_BLACK);
    display_draw_text_small_abs(0, GRAPH_Y + GRAPH_H + 4, DISPLAY_WHITE, stat);
#else
    snprintf(stat, sizeof(stat), "Cur: %d dBm        ", (int)rssi);
    display_draw_text_small_abs(0, GRAPH_Y + GRAPH_H + 17, DISPLAY_WHITE, stat);
#endif
    display_update_buffer();
}
