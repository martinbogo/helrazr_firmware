/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "menu.h"
#include "button.h"
#include "display.h"
#include "gps.h"
#include "theme.h"

// Cursor position as a 0-based index into the *visible* menu list (below).
static int selected = 0;

// Label per AppMode, indexed by enum value.
static const char* LABELS[] = {
    "",            // MODE_MENU
    "Status",
    "Spectrum",
    "Waterfall",
    "Noise",
    "Scanner",
    "Monitor",
    "DutyCycle",
    "FreqOffset",
    "Decoder",
    "Nodes",
    "Stats",
    "AutoTrack",
    "Standby",
    "OTA Update",
    "GPS",
    "Settings",
};

// Menu display order, independent of enum order. Edit this to reorder the menu.
// GPS sits right after Status so it lands on the first page on both displays;
// Settings sits near the end.
static const AppMode MENU_ORDER[] = {
    MODE_STATUS, MODE_GPS, MODE_SPECTRUM, MODE_WATERFALL, MODE_NOISE,
    MODE_SCANNER, MODE_MONITOR, MODE_DUTY, MODE_FREQOFFSET, MODE_DECODER,
    MODE_NODES, MODE_STATS, MODE_AUTOTRACK, MODE_SETTINGS, MODE_STANDBY, MODE_OTA,
};
static const int MENU_ORDER_COUNT = sizeof(MENU_ORDER) / sizeof(MENU_ORDER[0]);

static const char* menu_label(AppMode m) { return LABELS[m]; }

// A mode is hidden when its hardware/feature isn't available.
static bool mode_visible(AppMode m) {
    if (m == MODE_GPS) {
#if HAS_GPS
        return gps_present();
#else
        return false;
#endif
    }
    return true;
}

// Builds the currently-visible ordered list into `out`; returns its length.
static int build_visible(AppMode *out) {
    int n = 0;
    for (int i = 0; i < MENU_ORDER_COUNT; i++)
        if (mode_visible(MENU_ORDER[i])) out[n++] = MENU_ORDER[i];
    return n;
}

static int last_page = -1;

void menu_init() {
    last_page = -1; // force clear when entering menu
}

void menu_update() {
    AppMode vis[MODE_COUNT];
    int n = build_visible(vis);
    if (selected >= n) selected = n - 1;   // list may have shrunk (GPS appeared/left)
    if (selected < 0)  selected = 0;
    if (button_short_pressed()) {
        selected++;
        if (selected >= n) selected = 0;
    } else if (button_double_pressed()) {
        selected--;
        if (selected < 0) selected = n - 1;
    }
}

void menu_draw() {
#if HAS_OLED
    const int MAX_ROWS = 4;
#else
    const int MAX_ROWS = 5;
#endif
    const int ITEMS_PER_PAGE = MAX_ROWS * 2;
    int page = selected / ITEMS_PER_PAGE;

#if HAS_OLED
    display_clear();
#else
    if (page != last_page) {
        display_clear(true);
    }
#endif
    last_page = page;

    ui_header("Select Mode");

    AppMode vis[MODE_COUNT];
    int n = build_visible(vis);
    int startIndex = page * ITEMS_PER_PAGE;
    int endIndex = startIndex + ITEMS_PER_PAGE - 1;
    if (endIndex >= n) endIndex = n - 1;

    for (int row = 0; row < MAX_ROWS; row++) {
#if HAS_OLED
        int y = 14 + row * 9;
        for (int col = 0; col < 2; col++) {
            int i = startIndex + col * MAX_ROWS + row;
            int x = col * 64 + 2;
            if (i <= endIndex) {
                if (i == selected) {
                    display_fill_rect_abs(col * 64, y - 1, 62, 9, DISPLAY_CYAN);
                    display_draw_text_small_abs(x, y, DISPLAY_BLACK, menu_label(vis[i]));
                } else {
                    display_draw_text_small_abs(x, y, DISPLAY_WHITE, menu_label(vis[i]));
                }
            }
        }
#else
        int y = 36 + row * 17;
        display_begin_line(y, false);
        for (int col = 0; col < 2; col++) {
            int i = startIndex + col * MAX_ROWS + row;
            int cx = col * 120;
            if (i <= endIndex) {
                if (i == selected) {
                    display_line_fill_rect(cx, 118, DISPLAY_CYAN);
                    display_line_text(cx + 4, DISPLAY_BLACK, menu_label(vis[i]));
                } else {
                    display_line_fill_rect(cx, 118, DISPLAY_BLACK);
                    display_line_text(cx + 4, DISPLAY_WHITE, menu_label(vis[i]));
                }
            } else {
                display_line_fill_rect(cx, 118, DISPLAY_BLACK);
            }
        }
        display_end_line();
#endif
    }

    ui_footer("S:next  D:prev  L:select");

    display_update_buffer();
}

AppMode menu_selection() {
    AppMode vis[MODE_COUNT];
    int n = build_visible(vis);
    if (selected < 0)  selected = 0;
    if (selected >= n) selected = n - 1;
    return vis[selected];
}

#define NUM_DROPS 15
struct MatrixDrop {
    int x;
    float y;
    float speed;
};
static MatrixDrop drops[NUM_DROPS];
static bool matrix_initialized = false;

void menu_reset_matrix() {
    matrix_initialized = false;
}

void menu_draw_matrix() {
    if (!matrix_initialized) {
        for (int i=0; i<NUM_DROPS; i++) {
#if HAS_OLED
            drops[i].x = random(0, 128);
            drops[i].y = random(-64, 0);
#else
            drops[i].x = random(0, 240);
            drops[i].y = random(-135, 0);
#endif
            drops[i].speed = random(10, 40) / 10.0f;
        }
        matrix_initialized = true;
    }

#if HAS_OLED
    display_clear();
    int height = 64;
#else
    display_clear(true);
    int height = 135;
#endif

    for (int i=0; i<NUM_DROPS; i++) {
        drops[i].y += drops[i].speed;
        if (drops[i].y > height) {
            drops[i].y = random(-20, 0);
#if HAS_OLED
            drops[i].x = random(0, 128);
#else
            drops[i].x = random(0, 240);
#endif
            drops[i].speed = random(10, 40) / 10.0f;
        }

        int drawY = (int)drops[i].y;
        if (drawY >= 0 && drawY < height) {
            display_draw_vline(drops[i].x, drawY, 3, DISPLAY_CYAN);
        }
    }
    display_update_buffer();
}
