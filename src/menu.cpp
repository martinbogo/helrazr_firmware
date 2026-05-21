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
    "Decoder",
    "Nodes",
    "Stats",
    "AutoTrack",
    "Standby",
    "OTA Update",
};

void menu_init() { /* retained state: selected is not reset */ }

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
    display_clear();
#if HAS_OLED
    display_draw_text_abs(25, 0, DISPLAY_CYAN, "Select Mode");
    display_draw_hline(0, 10, 128, DISPLAY_GRAY);
#else
    display_draw_text_abs(55, 15, DISPLAY_CYAN, "Select Mode");
    display_draw_hline(0, 20, 240, DISPLAY_GRAY);
#endif

#if HAS_OLED
    const int MAX_ROWS = 4;
#else
    const int MAX_ROWS = 5;
#endif
    const int ITEMS_PER_PAGE = MAX_ROWS * 2;
    int page = (selected - 1) / ITEMS_PER_PAGE;
    int startIndex = page * ITEMS_PER_PAGE + 1;
    int endIndex = startIndex + ITEMS_PER_PAGE - 1;
    if (endIndex >= MODE_COUNT) endIndex = MODE_COUNT - 1;

    for (int i = startIndex; i <= endIndex; i++) {
        int displayIdx = i - startIndex;
        int col = displayIdx / MAX_ROWS;
        int row = displayIdx % MAX_ROWS;
#if HAS_OLED
        int x = col * 64 + 2;
        int y = 14 + row * 9;
        
        if (i == selected) {
            display_fill_rect_abs(col * 64, y - 1, 62, 9, DISPLAY_CYAN);
            display_draw_text_small_abs(x, y, DISPLAY_BLACK, LABELS[i]);
        } else {
            display_draw_text_small_abs(x, y, DISPLAY_WHITE, LABELS[i]);
        }
#else
        int x = col * 120 + 4;
        int y = 36 + row * 17;
        
        if (i == selected) {
            display_fill_rect_abs(col * 120, y - 13, 118, 16, DISPLAY_CYAN);
            display_draw_text_abs(x, y, DISPLAY_BLACK, LABELS[i]);
        } else {
            display_fill_rect_abs(col * 120, y - 13, 118, 16, DISPLAY_BLACK);
            display_draw_text_abs(x, y, DISPLAY_WHITE, LABELS[i]);
        }
#endif
    }
    
#if HAS_OLED
    display_draw_text_small_abs(0, 56, DISPLAY_CYAN, "S:nx  D:pv  L:sel");
#else
    display_draw_text_small_abs(4, 126, DISPLAY_CYAN, "S:nxt  D:prv  L:sel");
#endif
    
    display_update_buffer();
}

AppMode menu_selection() { return (AppMode)selected; }
