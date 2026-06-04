/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#pragma once
#include <Arduino.h>

// -----------------------------------------------------------------------------
// Resolution-aware layout metrics.
//
// This is the SINGLE place that per-display geometry lives. Screens express
// layout in terms of these metrics (header band, content area, body rows,
// columns) instead of hardcoded pixel numbers, so supporting a new panel is a
// matter of adding one profile block below, not editing every screen.
//
// To add a resolution: add an `#elif` with that panel's HAS_* guard defining
// LCD_W/LCD_H and the five band constants. Everything else derives from them.
// -----------------------------------------------------------------------------

#if HAS_TFT
  // 240x135 colour TFT (ST7789)
  #define LCD_W 240
  #define LCD_H 135
  static const int UI_HEADER_H = 24;  // accent title + 2px rule, content starts here
  static const int UI_FOOTER_H = 13;  // dim hint strip at the bottom
  static const int UI_ROW_H    = 18;  // FreeSans body row pitch
  static const int UI_PAD      = 6;   // horizontal edge padding
  static const int UI_ROW_BASE = 14;  // baseline offset within a row (FreeSans)
#else
  // 128x64 monochrome OLED (SSD1306)
  #define LCD_W 128
  #define LCD_H 64
  static const int UI_HEADER_H = 11;  // small title + divider
  static const int UI_FOOTER_H = 9;   // hint line
  static const int UI_ROW_H    = 10;  // small-font row pitch
  static const int UI_PAD      = 0;
  static const int UI_ROW_BASE = 1;   // top-origin font: small offset into the row
#endif

// Content band (between header and footer).
static inline int ui_content_top()    { return UI_HEADER_H; }
static inline int ui_content_bottom() { return LCD_H - UI_FOOTER_H; }
static inline int ui_content_h()      { return ui_content_bottom() - ui_content_top(); }

// Number of body rows that fit, and the y of body row `i`.
// On TFT this returns a FreeSans baseline; on OLED a top-left origin.
static inline int ui_body_rows() { return ui_content_h() / UI_ROW_H; }
static inline int ui_row_y(int i) {
    return ui_content_top() + i * UI_ROW_H + UI_ROW_BASE;
}

// x of column `col` in an `ncols`-wide grid, and that column's width.
static inline int ui_col_x(int col, int ncols) {
    return UI_PAD + col * ((LCD_W - 2 * UI_PAD) / ncols);
}
static inline int ui_col_w(int ncols) {
    return (LCD_W - 2 * UI_PAD) / ncols;
}

// y of the footer hint baseline/top.
static inline int ui_footer_y() { return LCD_H - UI_FOOTER_H + 2; }
