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

#if HAS_TFT
#include <Adafruit_ST7789.h>
extern Adafruit_ST7789 tft;
#elif HAS_OLED
#include <Adafruit_SSD1306.h>
extern Adafruit_SSD1306 tft;
#endif

// Generic color definitions for both displays (SSD1306 uses 1 for white, 0 for black)
#if HAS_OLED
#define DISPLAY_BLACK SSD1306_BLACK
#define DISPLAY_WHITE SSD1306_WHITE
#define DISPLAY_CYAN  SSD1306_WHITE
#define DISPLAY_GREEN SSD1306_WHITE
#define DISPLAY_YELLOW SSD1306_WHITE
#define DISPLAY_RED   SSD1306_WHITE
#define DISPLAY_GRAY  SSD1306_WHITE
#else
#define DISPLAY_BLACK 0x0000
#define DISPLAY_WHITE 0xFFFF
#define DISPLAY_CYAN  0x07FF
#define DISPLAY_GREEN 0x07E0
#define DISPLAY_YELLOW 0xFFE0
#define DISPLAY_RED   0xF800
#define DISPLAY_GRAY  0x4228
#endif

// Lifecycle
void display_init();
void display_on();
void display_off();
bool display_is_on();

// Status mode
void display_update(float lat, float lon, int sats, bool gps_fix,
                    float lora_rssi, float lora_snr, int lora_packets,
                    bool lora_listening, float bat_voltage, uint32_t uptime_s);

// General drawing primitives (for modes)
void display_clear();
void display_update_buffer(); // for OLED page updates
void display_draw_text(int x, int y, uint16_t color, const char* text);
void display_fill_rect(int x, int y, int w, int h, uint16_t color);
void display_draw_hline(int x, int y, int w, uint16_t color);
void display_draw_vline(int x, int y, int h, uint16_t color);
void display_draw_line(int x0, int y0, int x1, int y1, uint16_t color);

void display_draw_text_abs(int x, int y, uint16_t color, const char* text);
void display_draw_text_small_abs(int x, int y, uint16_t color, const char* text);
void display_draw_text_tiny_abs(int x, int y, uint16_t color, const char* text);
void display_fill_rect_abs(int x, int y, int w, int h, uint16_t color);
void display_draw_text_small(int x, int y, uint16_t color, const char* text);


