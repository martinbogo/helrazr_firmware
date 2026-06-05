/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "settings.h"
#include "display.h"
#include "layout.h"
#include "theme.h"
#include "prefs.h"
#if HAS_TFT
#include <Fonts/FreeSans9pt7b.h>
#endif

// A settings row: a name, a function that fills its current value, and a
// function that advances/cycles that value. Append rows to add settings.
struct SettingItem {
    const char *name;
    void (*value)(char *buf, size_t n);
    void (*change)();
};

static void theme_value(char *b, size_t n)  { snprintf(b, n, "%s", theme_name(theme_current())); }
static void theme_change()                  { theme_next(); }
static void units_value(char *b, size_t n)  { snprintf(b, n, "%s", units_name(prefs_units())); }
static void units_change()                  { prefs_set_units(prefs_units() + 1); }
static void coords_value(char *b, size_t n) { snprintf(b, n, "%s", coords_name(prefs_coords())); }
static void coords_change()                 { prefs_set_coords(prefs_coords() + 1); }
#if HAS_TFT
static void bright_value(char *b, size_t n) { snprintf(b, n, "%d%%", prefs_brightness()); }
static void bright_change() {
    int b = prefs_brightness();
    b = (b >= 100) ? 25 : b + 25;       // cycle 25/50/75/100
    prefs_set_brightness(b);
    theme_set(theme_current());          // re-apply so the backlight updates now
}
#endif

static const SettingItem ITEMS[] = {
    { "Theme",  theme_value,  theme_change },
    { "Units",  units_value,  units_change },
    { "Coords", coords_value, coords_change },
#if HAS_TFT
    { "Bright", bright_value, bright_change },
#endif
};
static const int ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);

static int  cursor = 0;
static bool needClear = true;

void settings_enter() { cursor = 0; needClear = true; }

void settings_short_press()  { cursor = (cursor + 1) % ITEM_COUNT; needClear = true; }
void settings_double_press() { ITEMS[cursor].change(); needClear = true; }

// Draws one row, highlighted if selected. Transparent text so the highlight
// bar shows through (display_draw_text_abs forces a bg, so draw TFT manually).
static void draw_row(int i) {
    char val[20], row[36];
    ITEMS[i].value(val, sizeof(val));
    snprintf(row, sizeof(row), "%s: %s", ITEMS[i].name, val);
    int y = ui_row_y(i);
    bool sel = (i == cursor);
#if HAS_OLED
    if (sel) display_fill_rect_abs(0, y - 1, LCD_W, UI_ROW_H - 1, DISPLAY_CYAN);
    display_draw_text_small_abs(UI_PAD, y, sel ? DISPLAY_BLACK : DISPLAY_WHITE, row);
#else
    if (sel) tft.fillRect(0, y - 14, LCD_W, UI_ROW_H, DISPLAY_CYAN);
    tft.setFont(&FreeSans9pt7b);
    tft.setTextSize(1);
    tft.setTextColor(sel ? DISPLAY_BLACK : DISPLAY_WHITE); // transparent bg
    tft.setCursor(UI_PAD, y);
    tft.print(row);
#endif
}

void settings_update() {
    if (!needClear) return; // nothing here changes on its own; redraw only on input
    needClear = false;

#if HAS_OLED
    display_clear();
#else
    display_clear(true);
#endif
    ui_header("Settings");
    for (int i = 0; i < ITEM_COUNT; i++) draw_row(i);
    ui_footer("S:item  D:change  L:back");
#if HAS_OLED
    display_update_buffer();
#endif
}
