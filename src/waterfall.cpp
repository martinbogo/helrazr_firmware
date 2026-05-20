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
static bool isPaused = false;

static uint16_t rssiToWaterfallColor(int rssi) {
    if (rssi < -120) return DISPLAY_BLACK;
    if (rssi < -100) return DISPLAY_GRAY;
    if (rssi < -85)  return DISPLAY_GREEN;
    if (rssi < -70)  return DISPLAY_YELLOW;
    return DISPLAY_RED;
}

void waterfall_short_press() {
    isPaused = !isPaused;
}

void waterfall_enter() {
    memset(history, 0, sizeof(history));
    nextRow = 0;
    isPaused = false;

    display_clear();
#if HAS_OLED
    display_draw_text_abs(5, 0, DISPLAY_CYAN, "Waterfall 902-928");
    display_draw_text_small_abs(0,   GRAPH_Y + GRAPH_H + 4, DISPLAY_CYAN, "02");
    display_draw_text_small_abs(54,  GRAPH_Y + GRAPH_H + 4, DISPLAY_CYAN, "15");
    display_draw_text_small_abs(115, GRAPH_Y + GRAPH_H + 4, DISPLAY_CYAN, "28");
#else
    display_draw_text_abs(30, 15, DISPLAY_CYAN, "Waterfall 902-928 MHz");
    display_draw_text_small_abs(0,   GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "902");
    display_draw_text_small_abs(54,  GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "909");
    display_draw_text_small_abs(113, GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "916");
    display_draw_text_small_abs(172, GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "923");
    display_draw_text_small_abs(213, GRAPH_Y + GRAPH_H + 5, DISPLAY_CYAN, "928");
#endif
    display_update_buffer();
}

void waterfall_update() {
    if (isPaused) {
        // Just redraw the status
        return;
    }

    // Capture one row of data
    for (int i = 0; i < NUM_STEPS; i++) {
        float freq = FREQ_START + i * FREQ_STEP;
        float rssi = lora_scan_rssi(freq);
        
        int val = (int)rssi + 135;
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        history[nextRow][i] = val;
    }

    nextRow = (nextRow + 1) % GRAPH_H;

    // Draw the entire buffer
    for (int r = 0; r < GRAPH_H; r++) {
        // Map logical row (newest at bottom or top?)
        // Let's make newest row at the top (r=0), oldest at bottom (r=GRAPH_H-1)
        int histRow = (nextRow - 1 - r + GRAPH_H) % GRAPH_H;
        
        int y = GRAPH_Y + r;
        
        for (int i = 0; i < NUM_STEPS; i++) {
            int x0 = GRAPH_X + (i       * GRAPH_W) / NUM_STEPS;
            int x1 = GRAPH_X + ((i + 1) * GRAPH_W) / NUM_STEPS;
            int bw = x1 - x0;
            if (bw < 1) bw = 1;
            
            int rssi = history[histRow][i] - 135;
            uint16_t color = rssiToWaterfallColor(rssi);
            
            display_fill_rect_abs(x0, y, bw, 1, color);
        }
    }

    // Pause annotation
    char status[36];
    snprintf(status, sizeof(status), isPaused ? "[Paused]" : "Running");
#if HAS_OLED
    display_fill_rect_abs(0, GRAPH_Y + GRAPH_H + 2, 40, 10, DISPLAY_BLACK);
    if(isPaused) display_draw_text_small_abs(0, GRAPH_Y + GRAPH_H + 4, DISPLAY_WHITE, status);
#else
    display_fill_rect_abs(0, GRAPH_Y + GRAPH_H + 15, 60, 12, DISPLAY_BLACK);
    if(isPaused) display_draw_text_small_abs(0, GRAPH_Y + GRAPH_H + 17, DISPLAY_WHITE, status);
#endif

    display_update_buffer();
}
