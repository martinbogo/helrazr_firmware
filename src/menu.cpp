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

static int selected = 1;

static const char* LABELS[] = {
    "",
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
};

static int last_page = -1;

void menu_init() {
    last_page = -1; // force clear when entering menu
}

void menu_update() {
    if (button_short_pressed()) {
        selected++;
        if (selected >= MODE_COUNT) selected = 1;
    } else if (button_double_pressed()) {
        selected--;
        if (selected <= 0) selected = MODE_COUNT - 1;
    }
}

void menu_draw() {
#if HAS_OLED
    const int MAX_ROWS = 4;
#else
    const int MAX_ROWS = 5;
#endif
    const int ITEMS_PER_PAGE = MAX_ROWS * 2;
    int page = (selected - 1) / ITEMS_PER_PAGE;

#if HAS_OLED
    display_clear();
#else
    if (page != last_page) {
        display_clear(true);
    }
#endif
    last_page = page;

#if HAS_OLED
    display_draw_text_abs(25, 0, DISPLAY_CYAN, "Select Mode");
    display_draw_hline(0, 10, 128, DISPLAY_GRAY);
#else
    display_draw_text_line(55, 15, DISPLAY_CYAN, "Select Mode");
    display_draw_hline(0, 20, 240, DISPLAY_GRAY);
#endif

    int startIndex = page * ITEMS_PER_PAGE + 1;
    int endIndex = startIndex + ITEMS_PER_PAGE - 1;
    if (endIndex >= MODE_COUNT) endIndex = MODE_COUNT - 1;

    for (int row = 0; row < MAX_ROWS; row++) {
#if HAS_OLED
        int y = 14 + row * 9;
        for (int col = 0; col < 2; col++) {
            int i = startIndex + col * MAX_ROWS + row;
            int x = col * 64 + 2;
            if (i <= endIndex) {
                if (i == selected) {
                    display_fill_rect_abs(col * 64, y - 1, 62, 9, DISPLAY_CYAN);
                    display_draw_text_small_abs(x, y, DISPLAY_BLACK, LABELS[i]);
                } else {
                    display_draw_text_small_abs(x, y, DISPLAY_WHITE, LABELS[i]);
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
                    display_line_text(cx + 4, DISPLAY_BLACK, LABELS[i]);
                } else {
                    display_line_fill_rect(cx, 118, DISPLAY_BLACK);
                    display_line_text(cx + 4, DISPLAY_WHITE, LABELS[i]);
                }
            } else {
                display_line_fill_rect(cx, 118, DISPLAY_BLACK);
            }
        }
        display_end_line();
#endif
    }

#if HAS_OLED
    display_draw_text_small_abs(0, 56, DISPLAY_CYAN, "S:nx  D:pv  L:sel");
#else
    display_draw_text_small_line(4, 126, DISPLAY_CYAN, "S:nxt  D:prv  L:sel");
#endif

    display_update_buffer();
}

AppMode menu_selection() { return (AppMode)selected; }

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
