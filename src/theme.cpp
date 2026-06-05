/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "theme.h"
#include "display.h"
#include "prefs.h"

#if defined(ESP32)
#include <Preferences.h>
static Preferences themePrefs;
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;
static const char *THEME_PATH = "/theme.dat";
#endif

// A palette in RGB565. `oledDim` lowers OLED brightness (e.g. for night use).
struct ThemeDef {
    const char *name;
    uint16_t black, white, accent, green, yellow, red, gray;
    bool oledDim;
    bool brightUi;  // secondary UI (compass rose) renders bright white
};

// To add a theme, append a row. Order is the cycle order. The colour TFT gets
// the full palette set; the monochrome OLED can only vary brightness, so it
// gets just two modes (Day = full, Night = dim).
#if HAS_OLED
static const ThemeDef THEMES[] = {
    // name     (colour fields unused on OLED)  dim    bright
    { "Day",    0, 0, 0, 0, 0, 0, 0,            false, true  },
    { "Night",  0, 0, 0, 0, 0, 0, 0,            true,  false },
};
#else
static const ThemeDef THEMES[] = {
    // name        black   white   accent  green   yellow  red     gray    dim    bright
    { "Hiking",    0x0000, 0xFFFF, 0x07E0, 0x07E0, 0xFD20, 0xF800, 0x8C71, false, true  },
    { "Night",     0x0000, 0xF800, 0xF800, 0xF800, 0xB000, 0xF800, 0x6000, true,  false },
    { "Amber",     0x0000, 0xFD20, 0xFD20, 0x07E0, 0xFD20, 0xF800, 0x8410, false, false },
    { "Aviation",  0x0000, 0xFFFF, 0x07FF, 0x07E0, 0xFFE0, 0xF800, 0x8410, false, true  },
    { "Mono",      0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x8410, false, true  },
};
#endif
static const int THEME_N = sizeof(THEMES) / sizeof(THEMES[0]);

// RGB565 palette globals. TFT only; OLED is monochrome (tokens are macros).
#if HAS_TFT
uint16_t DISPLAY_BLACK  = 0x0000;
uint16_t DISPLAY_WHITE  = 0xFFFF;
uint16_t DISPLAY_CYAN   = 0x07E0;
uint16_t DISPLAY_GREEN  = 0x07E0;
uint16_t DISPLAY_YELLOW = 0xFD20;
uint16_t DISPLAY_RED    = 0xF800;
uint16_t DISPLAY_GRAY   = 0x8C71;
#endif

static int s_theme = 0;

static void apply() {
    const ThemeDef &t = THEMES[s_theme];
#if HAS_TFT
    DISPLAY_BLACK  = t.black;
    DISPLAY_WHITE  = t.white;
    DISPLAY_CYAN   = t.accent;
    DISPLAY_GREEN  = t.green;
    DISPLAY_YELLOW = t.yellow;
    DISPLAY_RED    = t.red;
    DISPLAY_GRAY   = t.gray;
    // Night themes also dim the TFT backlight; others honour the brightness pref.
    display_set_backlight(t.oledDim ? 30 : (uint8_t)prefs_brightness());
#endif
#if HAS_OLED
    // Monochrome panel: themes can't change colour, so a "dim" theme just lowers
    // brightness. Set the contrast to a SAFE floor -- never 0, which on the
    // Heltec SSD1306 blanks the panel and makes the UI unusable.
    tft.ssd1306_command(SSD1306_SETCONTRAST);
    tft.ssd1306_command(t.oledDim ? 0x50 : 0xFF); // 0x50 = dim but readable, 0xFF = max brightness
#endif
}

static void load() {
#if defined(ESP32)
    themePrefs.begin("theme", true);
    s_theme = themePrefs.getInt("idx", 0);
    themePrefs.end();
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
    InternalFS.begin();
    File f(InternalFS);
    if (f.open(THEME_PATH, FILE_O_READ)) {
        s_theme = f.read();
        f.close();
    }
#endif
    if (s_theme < 0 || s_theme >= THEME_N) s_theme = 0;
}

static void store() {
#if defined(ESP32)
    themePrefs.begin("theme", false);
    themePrefs.putInt("idx", s_theme);
    themePrefs.end();
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
    InternalFS.remove(THEME_PATH);
    File f(InternalFS);
    if (f.open(THEME_PATH, FILE_O_WRITE)) {
        uint8_t b = (uint8_t)s_theme;
        f.write(&b, 1);
        f.close();
    }
#endif
}

void theme_init() { load(); apply(); }

int         theme_count()      { return THEME_N; }
const char* theme_name(int i)  { return (i >= 0 && i < THEME_N) ? THEMES[i].name : "?"; }
int         theme_current()    { return s_theme; }

void theme_set(int i) {
    if (i < 0) i = 0;
    if (i >= THEME_N) i = THEME_N - 1;
    s_theme = i;
    apply();
    store();
}

void theme_next() { theme_set((s_theme + 1) % THEME_N); }
bool theme_bright_ui() { return THEMES[s_theme].brightUi; }
