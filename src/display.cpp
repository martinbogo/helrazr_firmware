/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "display.h"
#include "pins.h"
#include "gps.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/Picopixel.h>

#if HAS_TFT

static SPIClass displaySPI(NRF_SPIM3, 9, PIN_TFT_SCK, PIN_TFT_MOSI);
Adafruit_ST7789 tft(&displaySPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

#elif HAS_OLED

Adafruit_SSD1306 tft(128, 64, &Wire, PIN_OLED_RST);

#endif

static bool powered = false;

#if HAS_TFT
static GFXcanvas16 _tftLine(240, 18);
static int _tftLineTopY = 0;
static int _tftLineH = 18;
static bool _tftLineSmall = false;
#else
static int _lineOrigY = 0;
static bool _lineIsSmall = false;
#endif

static void fmtFloat(char* buf, int buflen, float val, int width, int prec) {
    String s(val, prec);
    while ((int)s.length() < width) s = " " + s;
    s.toCharArray(buf, buflen);
}

void display_init() {
#if HAS_TFT
    pinMode(PIN_TFT_PWR, OUTPUT);
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_PWR, LOW);
    delay(100);

    displaySPI.begin();
    tft.init(135, 240);
    tft.setRotation(3);
    tft.invertDisplay(true);
    tft.fillScreen(DISPLAY_BLACK);
    tft.setFont(&FreeSans9pt7b);
    tft.setTextSize(1);
    tft.setTextWrap(false);

    digitalWrite(PIN_TFT_BL, LOW);
#elif HAS_OLED
#if defined(HELTEC_V3) || defined(HELTEC_V4)
    // Core esp32-s3-devkitc-1 variant doesn't auto-enable Vext (Pin 36),
    // and V4 relies on this to power the OLED.
    pinMode(36, OUTPUT);
    digitalWrite(36, LOW); // Active LOW for power on
    delay(50);
#endif
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    tft.begin(SSD1306_SWITCHCAPVCC, 0x3C);

    // The SSD1306 SETCONTRAST command has a very narrow visual range on its own.
    // To achieve deeper dimming, we must also lower the pre-charge voltage period.
    tft.ssd1306_command(SSD1306_SETPRECHARGE);
    tft.ssd1306_command(0xF1); // Phase1=1, Phase2=15 DCLKs

    tft.ssd1306_command(SSD1306_SETVCOMDETECT);
    tft.ssd1306_command(0x40);

    tft.ssd1306_command(SSD1306_SETCONTRAST);
    tft.ssd1306_command(255);

    tft.clearDisplay();
    tft.setTextColor(DISPLAY_WHITE);
    tft.setTextSize(1);
    tft.setTextWrap(false);
#endif

    powered = true;
    display_draw_text(10, 15, DISPLAY_CYAN, "Starting...");
    display_update_buffer();
}

void display_on() {
#if HAS_TFT
    digitalWrite(PIN_TFT_PWR, LOW);
    delay(50);
    digitalWrite(PIN_TFT_BL, LOW);
#elif HAS_OLED
    tft.ssd1306_command(SSD1306_DISPLAYON);
#endif
    powered = true;
}

void display_off() {
#if HAS_TFT
    digitalWrite(PIN_TFT_BL, HIGH);
    digitalWrite(PIN_TFT_PWR, HIGH);
#elif HAS_OLED
    tft.ssd1306_command(SSD1306_DISPLAYOFF);
#endif
    powered = false;
}

bool display_is_on() { return powered; }

void display_update(float lat, float lon, int sats, bool gps_fix,
                    float lora_rssi, float lora_snr, int lora_packets,
                    bool lora_listening, float bat_voltage, uint32_t uptime_s) {
    if (!powered) return;

#if HAS_OLED
    display_clear();
#endif

    char buf[40];

#if HAS_TFT
    const int lineH = 18;
    int y = 2;

    auto drawLine = [&](int yPos, uint16_t color, const char* text) {
        _tftLine.fillScreen(DISPLAY_BLACK);
        _tftLine.setFont(&FreeSans9pt7b);
        _tftLine.setTextSize(1);
        _tftLine.setTextColor(color);
        _tftLine.setCursor(0, 13);
        _tftLine.print(text);
        tft.drawRGBBitmap(0, yPos, _tftLine.getBuffer(), 240, 18);
    };

    auto drawLineTwoCol = [&](int yPos, uint16_t c1, const char* t1,
                               uint16_t c2, const char* t2, int x2) {
        _tftLine.fillScreen(DISPLAY_BLACK);
        _tftLine.setFont(&FreeSans9pt7b);
        _tftLine.setTextSize(1);
        _tftLine.setTextColor(c1);
        _tftLine.setCursor(0, 13);
        _tftLine.print(t1);
        _tftLine.setTextColor(c2);
        _tftLine.setCursor(x2, 13);
        _tftLine.print(t2);
        tft.drawRGBBitmap(0, yPos, _tftLine.getBuffer(), 240, 18);
    };

#if HAS_GPS
    snprintf(buf, sizeof(buf), "GPS: %-3s  Sats: %d  Rx: %lu", gps_fix ? "3D" : "No", sats, gps_chars_processed());
    drawLine(y, DISPLAY_GREEN, buf);
#endif
    y += lineH;

    char rssibuf[8];
    fmtFloat(rssibuf, sizeof(rssibuf), lora_rssi, 5, 1);
    snprintf(buf, sizeof(buf), "LoRa %s  RSSI: %s", lora_listening ? "RX" : "--", rssibuf);
    drawLine(y, DISPLAY_YELLOW, buf);
    y += lineH;

    char snrbuf[8];
    fmtFloat(snrbuf, sizeof(snrbuf), lora_snr, 4, 1);
    snprintf(buf, sizeof(buf), "SNR: %s   Pkts: %d", snrbuf, lora_packets);
    drawLine(y, DISPLAY_WHITE, buf);
    y += lineH;

    char batbuf[7];
    fmtFloat(batbuf, sizeof(batbuf), bat_voltage, 4, 2);
    snprintf(buf, sizeof(buf), "Bat: %sV", batbuf);
    uint16_t bat_color;
    if (bat_voltage >= 3.7f) bat_color = DISPLAY_GREEN;
    else if (bat_voltage >= 3.1f) bat_color = DISPLAY_YELLOW;
    else bat_color = DISPLAY_RED;

    uint32_t h = uptime_s / 3600, m = (uptime_s % 3600) / 60, s2 = uptime_s % 60;
    char ubuf[20];
    snprintf(ubuf, sizeof(ubuf), "Up: %02lu:%02lu:%02lu", h, m, s2);
    drawLineTwoCol(y, bat_color, buf, DISPLAY_WHITE, ubuf, 120);

#else  // OLED
    int y = 0;

#if HAS_GPS
    snprintf(buf, sizeof(buf), "GPS: %-3s  Sats: %d  Rx: %lu", gps_fix ? "3D" : "No", sats, gps_chars_processed());
    display_draw_text_abs(0, y, DISPLAY_GREEN, buf);
#endif
    y += 14;

    char rssibuf[8];
    fmtFloat(rssibuf, sizeof(rssibuf), lora_rssi, 5, 1);
    snprintf(buf, sizeof(buf), "LoRa %s  RSSI: %s", lora_listening ? "RX" : "--", rssibuf);
    display_draw_text_abs(0, y, DISPLAY_YELLOW, buf);
    y += 14;

    char snrbuf[8];
    fmtFloat(snrbuf, sizeof(snrbuf), lora_snr, 4, 1);
    snprintf(buf, sizeof(buf), "SNR:%s Pk:%d", snrbuf, lora_packets);
    display_draw_text_abs(0, y, DISPLAY_WHITE, buf);
    y += 14;

    char batbuf[7];
    fmtFloat(batbuf, sizeof(batbuf), bat_voltage, 4, 2);
    snprintf(buf, sizeof(buf), "Bat: %sV", batbuf);
    display_draw_text_abs(0, y, DISPLAY_WHITE, buf);

#if HAS_GPS
    y += 10;
    if (gps_fix) {
        char latbuf[8], lonbuf[8];
        fmtFloat(latbuf, sizeof(latbuf), lat, 6, 2);
        fmtFloat(lonbuf, sizeof(lonbuf), lon, 7, 2);
        snprintf(buf, sizeof(buf), "%s/%s", latbuf, lonbuf);
    } else {
        uint32_t h = uptime_s / 3600, m = (uptime_s % 3600) / 60, s2 = uptime_s % 60;
        snprintf(buf, sizeof(buf), "Up %02lu:%02lu:%02lu", h, m, s2);
    }
    display_draw_text_small_abs(0, y + 2, DISPLAY_CYAN, buf);
#endif

#endif  // HAS_TFT vs OLED

    display_update_buffer();
}

void display_clear(bool force_tft) {
#if HAS_TFT
    if (force_tft) {
        tft.fillScreen(DISPLAY_BLACK);
    }
#elif HAS_OLED
    tft.clearDisplay();
#endif
}

void display_update_buffer() {
#if HAS_OLED
    tft.display();
#endif
}

void display_draw_text(int x, int y, uint16_t color, const char* text) {
#if HAS_TFT
    tft.setFont(&FreeSans9pt7b);
    tft.setTextSize(1);
    tft.setTextColor(color, DISPLAY_BLACK);
    tft.setCursor(x, y);
#else
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(color, DISPLAY_BLACK);
    int mapped_x = (x * 128) / 240;
    int mapped_y = ((y - 13) * 64) / 135;
    if (mapped_y < 0) mapped_y = 0;
    if (mapped_y > 56) mapped_y = 56;
    tft.setCursor(mapped_x, mapped_y);
#endif
    tft.print(text);
}

void display_draw_text_small(int x, int y, uint16_t color, const char* text) {
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(color, DISPLAY_BLACK);
#if HAS_TFT
    tft.setCursor(x, y);
#else
    int mapped_x = (x * 128) / 240;
    int mapped_y = (y * 64) / 135;
    if (mapped_y > 56) mapped_y = 56;
    tft.setCursor(mapped_x, mapped_y);
#endif
    tft.print(text);
#if HAS_TFT
    tft.setFont(&FreeSans9pt7b);
#endif
}

void display_fill_rect(int x, int y, int w, int h, uint16_t color) {
#if HAS_TFT
    tft.fillRect(x, y, w, h, color);
#else
    int mapped_x = (x * 128) / 240;
    int mapped_y = (y * 64) / 135;
    int mapped_w = (w * 128) / 240;
    int mapped_h = (h * 64) / 135;
    if (mapped_w < 1) mapped_w = 1;
    if (mapped_h < 1) mapped_h = 1;
    tft.fillRect(mapped_x, mapped_y, mapped_w, mapped_h, color);
#endif
}

void display_draw_hline(int x, int y, int w, uint16_t color) {
    // All callers pass absolute pixel coordinates -- no mapping needed
    tft.drawFastHLine(x, y, w, color);
}

void display_draw_vline(int x, int y, int h, uint16_t color) {
    tft.drawFastVLine(x, y, h, color);
}

void display_draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
    tft.drawLine(x0, y0, x1, y1, color);
}

void display_draw_text_abs(int x, int y, uint16_t color, const char* text) {
#if HAS_TFT
    tft.setFont(&FreeSans9pt7b);
    tft.setTextSize(1);
    tft.setTextColor(color, DISPLAY_BLACK);  // TFT custom font ignores bg anyway
#else
    tft.setFont(NULL);
    tft.setTextSize(1);
    // OLED: transparent background -- buffer is cleared before each full redraw,
    // so we never need to erase old text. With a bg colour, black bg pixels would
    // overwrite any white highlight rect drawn beneath the text.
    tft.setTextColor(color);
#endif
    tft.setCursor(x, y);
    tft.print(text);
}

void display_draw_text_small_abs(int x, int y, uint16_t color, const char* text) {
    tft.setFont(NULL);
    tft.setTextSize(1);
#if HAS_TFT
    tft.setTextColor(color, DISPLAY_BLACK);  // TFT: bg clears old text on partial updates
#else
    tft.setTextColor(color);                 // OLED: transparent, same reason as above
#endif
    tft.setCursor(x, y);
    tft.print(text);
#if HAS_TFT
    tft.setFont(&FreeSans9pt7b);
#endif
}

void display_draw_text_tiny_abs(int x, int y, uint16_t color, const char* text) {
    tft.setFont(&Picopixel);
    tft.setTextSize(1);
    // Custom fonts use the baseline for y coordinate. Picopixel characters are ~5px tall,
    // so we shift y down by 5 to match the top-left coordinate system expected by callers.
    int baseline_y = y + 5;
#if HAS_TFT
    tft.setTextColor(color, DISPLAY_BLACK);
#else
    tft.setTextColor(color);
#endif
    tft.setCursor(x, baseline_y);
    tft.print(text);
#if HAS_TFT
    tft.setFont(&FreeSans9pt7b);
#else
    tft.setFont(NULL);
#endif
}

void display_fill_rect_abs(int x, int y, int w, int h, uint16_t color) {
    tft.fillRect(x, y, w, h, color);
}

void display_begin_line(int y, bool smallFont) {
#if HAS_TFT
    _tftLineSmall = smallFont;
    _tftLineH = smallFont ? 10 : 18;
    _tftLineTopY = smallFont ? y : (y - 13);
    _tftLine.fillScreen(DISPLAY_BLACK);
#else
    _lineOrigY = y;
    _lineIsSmall = smallFont;
#endif
}

void display_line_text(int x, uint16_t color, const char* text) {
#if HAS_TFT
    if (_tftLineSmall) {
        _tftLine.setFont(NULL);
        _tftLine.setTextSize(1);
        _tftLine.setTextColor(color);
        _tftLine.setCursor(x, 1);
    } else {
        _tftLine.setFont(&FreeSans9pt7b);
        _tftLine.setTextSize(1);
        _tftLine.setTextColor(color);
        _tftLine.setCursor(x, 13);
    }
    _tftLine.print(text);
#else
    if (_lineIsSmall)
        display_draw_text_small_abs(x, _lineOrigY, color, text);
    else
        display_draw_text_abs(x, _lineOrigY, color, text);
#endif
}

void display_line_fill_rect(int x, int w, uint16_t color) {
#if HAS_TFT
    _tftLine.fillRect(x, 0, w, _tftLineH, color);
#else
    int h = _lineIsSmall ? 8 : 14;
    tft.fillRect(x, _lineOrigY, w, h, color);
#endif
}

void display_end_line() {
#if HAS_TFT
    tft.drawRGBBitmap(0, _tftLineTopY, _tftLine.getBuffer(), 240, _tftLineH);
#endif
}

void display_draw_text_line(int x, int y, uint16_t color, const char* text) {
    display_begin_line(y, false);
    display_line_text(x, color, text);
    display_end_line();
}

void display_draw_text_small_line(int x, int y, uint16_t color, const char* text) {
    display_begin_line(y, true);
    display_line_text(x, color, text);
    display_end_line();
}
