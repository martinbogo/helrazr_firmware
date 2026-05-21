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
static bool isPaused = false;

#if !HAS_OLED
static int colorMode = 0; // 0=Classic, 1=Grayscale, 2=Ironbow, 3=Viridis, 4=Ocean, 5=Matrix, 6=Magma, 7=Super Bright
#endif

#if HAS_OLED
static int getOledLevel(int rssi) {
    if (rssi < -100) return 0; // Black background for typical noise floor
    if (rssi >= -70) return 4; // Solid white for strong signals
    if (rssi < -90) return 1;
    if (rssi < -80) return 2;
    return 3;
}
#endif

// Helper for RGB565 encoding
#define C565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

static uint16_t rssiToWaterfallColor(int rssi) {
#if HAS_OLED
    return DISPLAY_BLACK; // Replaced by per-pixel logic below
#else
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

    int level = 0;
    if (rssi >= -40) level = 7;
    else if (rssi >= -50) level = 6;
    else if (rssi >= -60) level = 5;
    else if (rssi >= -70) level = 4;
    else if (rssi >= -80) level = 3;
    else if (rssi >= -90) level = 2;
    else if (rssi >= -100) level = 1;

    return PALLETES[colorMode][level];
#endif
}

void waterfall_short_press() {
    isPaused = !isPaused;
#if HAS_OLED
    display_fill_rect_abs(112, 0, 16, 10, DISPLAY_BLACK);
    if (isPaused) {
        display_draw_text_small_abs(114, 0, DISPLAY_WHITE, "II");
    }
#else
    display_fill_rect_abs(220, 5, 20, 20, DISPLAY_BLACK);
    if (isPaused) {
        display_draw_text_abs(220, 18, DISPLAY_WHITE, "||");
    }
#endif
    display_update_buffer();
}

void waterfall_double_press() {
    isPaused = false; // Reset play state
#if !HAS_OLED
    colorMode = (colorMode + 1) % 8;
#endif
}

void waterfall_enter() {
    memset(history, 0, sizeof(history));
    nextRow = 0;
    totalScans = 0;
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
                    if (bayer2x2[y % 2][px % 2] < level) {
                        display_fill_rect_abs(px, y, 1, 1, DISPLAY_WHITE);
                    } else {
                        display_fill_rect_abs(px, y, 1, 1, DISPLAY_BLACK);
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

    display_update_buffer();
}
