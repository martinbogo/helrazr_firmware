/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "gpsview.h"
#include "display.h"
#include "gps.h"
#include "waypoints.h"
#include "button.h"
#include "theme.h"
#include <math.h>
#if HAS_TFT
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#endif

// Debounced "in motion" detection. GPS speed is noisy near walking pace, so a
// single threshold makes the compass flicker between the needle and MOVE. We
// smooth the speed (EMA), apply Schmitt-trigger thresholds, and require the new
// state to persist for a dwell time. The timing is asymmetric: quicker to
// engage the needle than to fall back to MOVE, so a brief pause or a momentary
// GPS dip doesn't drop the heading.
static bool     s_inMotion   = false;
static float    s_spdEma     = 0.0f;
static uint32_t s_dwellSince = 0;
static uint32_t s_lastEval   = 0;

static bool motion_state(float spd, bool courseValid) {
    uint32_t now = millis();
    if (s_lastEval == 0 || now - s_lastEval > 2000) { // first call / returned after a gap
        s_spdEma = spd;
        s_dwellSince = 0;
    }
    s_lastEval = now;
    s_spdEma += 0.35f * (spd - s_spdEma);

    const float    ENTER_KMH = 2.5f, EXIT_KMH = 1.0f; // Schmitt band
    const uint32_t ENTER_MS  = 1000, EXIT_MS  = 2500; // sustain before flipping

    bool target = s_inMotion ? (courseValid && s_spdEma > EXIT_KMH)
                             : (courseValid && s_spdEma > ENTER_KMH);
    if (target == s_inMotion)   s_dwellSince = 0;          // stable
    else if (s_dwellSince == 0) s_dwellSince = now;        // start dwell
    else if (now - s_dwellSince >= (target ? ENTER_MS : EXIT_MS)) {
        s_inMotion = target;                              // flip after dwell
        s_dwellSince = 0;
    }
    return s_inMotion;
}

// Needle inertia. The displayed heading is a damped angular spring chasing the
// (noisy) GPS target: it accelerates toward the shortest-way target, with
// damping, so it swings and settles like a real compass needle instead of
// snapping. Slightly underdamped (a little overshoot) for a natural feel.
struct NeedleSim { float angle, vel; uint32_t lastMs; bool init; };
static NeedleSim s_compassN  = { 0, 0, 0, false };
static NeedleSim s_wayfindN  = { 0, 0, 0, false };
static bool      s_needleAnimating = false; // true while a needle is still settling

static float needle_step(NeedleSim &n, float target) {
    uint32_t now = millis();
    if (!n.init || now - n.lastMs > 1000) {      // first use / returned after a gap: snap
        n.angle = target; n.vel = 0; n.lastMs = now; n.init = true;
        return n.angle;
    }
    float dt = (now - n.lastMs) / 1000.0f;
    n.lastMs = now;
    if (dt > 0.25f) dt = 0.25f;                  // cap to keep the integrator stable
    float err = fmodf(target - n.angle + 540.0f, 360.0f) - 180.0f; // shortest path [-180,180]
    const float K = 16.0f, C = 5.2f;             // stiffness / damping (~1.5s settle, slight swing)
    n.vel   += (K * err - C * n.vel) * dt;
    n.angle  = fmodf(n.angle + n.vel * dt + 360.0f, 360.0f);
    if (fabsf(err) > 0.5f || fabsf(n.vel) > 2.0f) s_needleAnimating = true;
    return n.angle;
}

enum GpsPage {
    PAGE_FIX = 0,
    PAGE_COMPASS,
    PAGE_WAYFINDER,
    PAGE_WAYPOINTS,
    PAGE_COUNT
};

static GpsPage  page = PAGE_FIX;
static bool     needClear = true;
static uint32_t lastDraw = 0;
static int      s_wf_target = 0; // wayfinder quick-goto cycle pointer

// Compass display modes (double-press on the Compass page cycles them).
enum CompassMode { CM_NORTH_UP = 0, CM_DIR_UP, CM_BUBBLE, CM_COUNT };
static CompassMode compassMode = CM_NORTH_UP;
#if HAS_OLED // only the OLED footer spells out the mode name; TFT shows it visually
static const char* compass_mode_name(CompassMode m) {
    switch (m) {
        case CM_NORTH_UP: return "North Up";
        case CM_DIR_UP:   return "Track Up";
        default:          return "Bubble";
    }
}
#endif

// Waypoint Manager (Page 4) sub-state.
enum WpState { WP_LIST = 0, WP_MENU };
static WpState  wpState  = WP_LIST;
static int      wpCursor = 0; // highlighted waypoint (stored/route order)
static int      wpMenuSel = 0;

static const char* WP_MENU_ITEMS[] = {
    "Go To", "Start Route", "Mark Here", "Average Here",
    "Move Up", "Move Down", "Delete", "Sort Nearest",
    "Sort Name", "Stop Nav", "Back",
};
static const int WP_MENU_COUNT = sizeof(WP_MENU_ITEMS) / sizeof(WP_MENU_ITEMS[0]);

static void wp_menu_execute(int sel);
static int  do_average();
static void clamp_wp_cursor();

// ---- TFT: full-screen offscreen canvas (flicker-free, FreeSans font) --------
//
// Every TFT page composes into this canvas and is blitted in one drawRGBBitmap,
// exactly like the Status screen's per-row canvas, so there is no flicker.
#if HAS_TFT
static GFXcanvas16 cv(240, 135);

static void T(int x, int y, uint16_t c, const char *s, uint8_t sz = 1) {
    cv.setFont(&FreeSans9pt7b);
    cv.setTextSize(sz);
    cv.setTextColor(c);
    cv.setCursor(x, y);
    cv.print(s);
}
static void Ts(int x, int y, uint16_t c, const char *s) { // small system font
    cv.setFont(NULL);
    cv.setTextSize(1);
    cv.setTextColor(c);
    cv.setCursor(x, y);
    cv.print(s);
}
#endif

// ---- lifecycle / input ------------------------------------------------------

void gpsview_enter() {
    page = PAGE_FIX;
    wpState = WP_LIST;
    needClear = true;
    lastDraw = 0;
    s_compassN.init = false;   // needles snap to current heading on entry
    s_wayfindN.init = false;
}

static void clamp_wp_cursor() {
    int n = wp_count();
    if (wpCursor >= n) wpCursor = n - 1;
    if (wpCursor < 0)  wpCursor = 0;
}

// Short press: cursor/menu navigation on the Waypoints page, else next page.
void gpsview_short_press() {
    needClear = true;
    if (page == PAGE_WAYPOINTS) {
        if (wpState == WP_LIST) {
            if (wp_count() > 0) wpCursor = (wpCursor + 1) % wp_count();
        } else {
            wpMenuSel = (wpMenuSel + 1) % WP_MENU_COUNT;
        }
        return;
    }
    page = (GpsPage)((page + 1) % PAGE_COUNT);
}

// Double press: page action / open-or-execute the context menu.
void gpsview_double_press() {
    needClear = true;
    if (page == PAGE_COMPASS) {
        compassMode = (CompassMode)((compassMode + 1) % CM_COUNT);
        return;
    }
    if (page == PAGE_WAYFINDER) {
        if (wp_count() > 0) { s_wf_target = (s_wf_target + 1) % wp_count(); nav_goto(s_wf_target); }
        return;
    }
    if (page == PAGE_WAYPOINTS) {
        if (wpState == WP_LIST) { wpState = WP_MENU; wpMenuSel = 0; }
        else                    { wp_menu_execute(wpMenuSel); }
        return;
    }
}

// Long press: close the context menu, else step back to page 1, else (false)
// let the main loop exit to the main menu.
bool gpsview_long_press() {
    if (page == PAGE_WAYPOINTS && wpState == WP_MENU) {
        wpState = WP_LIST; needClear = true; return true;
    }
    if (page != PAGE_FIX) { page = PAGE_FIX; needClear = true; return true; }
    return false;
}

// ---- shared helpers ---------------------------------------------------------

static const char* page_title() {
    switch (page) {
        case PAGE_FIX:       return "Fix / Sky";
        case PAGE_COMPASS:   return "Compass";
        case PAGE_WAYFINDER: return "Wayfinder";
        case PAGE_WAYPOINTS: return "Waypoints";
        default:             return "GPS";
    }
}

// Footer hint, action-aware and back-target-aware (Menu from first page, else Pg1).
static void footer_text(char *buf, size_t n, bool oled) {
    const char *back = (page == PAGE_FIX) ? "Menu" : "Pg1";
    const char *act  = "";
    if (page == PAGE_WAYFINDER) act = oled ? "D:Tgt " : "Dbl:Target  ";
    else if (page == PAGE_WAYPOINTS) act = oled ? "D:Mrk " : "Dbl:Mark  ";
    if (oled) snprintf(buf, n, "S:Pg %sL:%s", act, back);
    else      snprintf(buf, n, "Short:Page  %sLong:%s", act, back);
}

static uint16_t dop_color(float dop) {
    if (dop <= 0.0f) return DISPLAY_GRAY;
    if (dop < 2.0f)  return DISPLAY_GREEN;
    if (dop < 5.0f)  return DISPLAY_YELLOW;
    return DISPLAY_RED;
}

// Point on a circle as a compass bearing: 0 = North = up, clockwise.
static void polar(int cx, int cy, float r, float deg, int *px, int *py) {
    float a = radians(deg);
    *px = cx + (int)lroundf(r * sinf(a));
    *py = cy - (int)lroundf(r * cosf(a));
}

static const char* cardinal8(float h) {
    static const char* C[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    return C[((int)((h + 22.5f) / 45.0f)) & 7];
}

// Shortest signed angular difference, in (-180, 180].
static float wrap180(float a) { return fmodf(a + 540.0f, 360.0f) - 180.0f; }

// Arrow into any GFX surface (TFT canvas or OLED buffer both derive Adafruit_GFX).
static void draw_arrow(Adafruit_GFX &g, int cx, int cy, float r, float deg, uint16_t color) {
    int tx, ty, bl_x, bl_y, br_x, br_y;
    polar(cx, cy, r, deg, &tx, &ty);
    polar(cx, cy, r * 0.34f, deg + 140, &bl_x, &bl_y);
    polar(cx, cy, r * 0.34f, deg - 140, &br_x, &br_y);
    g.fillTriangle(tx, ty, bl_x, bl_y, br_x, br_y, color);
    g.fillCircle(cx, cy, 2, color);
}

// Bold up-pointing navigation arrow ("you are heading this way") with a concave
// notch at the base, like a GPS location cursor. Always points up (track-up).
static void draw_navarrow(Adafruit_GFX &g, int cx, int cy, int r, uint16_t color) {
    int tipY  = cy - (int)lroundf(r * 0.78f);
    int baseY = cy + (int)lroundf(r * 0.55f);
    int half  = (int)lroundf(r * 0.58f);
    int notchY = cy + (int)lroundf(r * 0.12f);
    g.fillTriangle(cx, tipY, cx - half, baseY, cx, notchY, color);
    g.fillTriangle(cx, tipY, cx + half, baseY, cx, notchY, color);
}

static float haversine_m(float lat1, float lon1, float lat2, float lon2) {
    const float R = 6371000.0f;
    float dlat = radians(lat2 - lat1), dlon = radians(lon2 - lon1);
    float a = sinf(dlat / 2) * sinf(dlat / 2) +
              cosf(radians(lat1)) * cosf(radians(lat2)) * sinf(dlon / 2) * sinf(dlon / 2);
    return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

static float bearing_deg(float lat1, float lon1, float lat2, float lon2) {
    float dlon = radians(lon2 - lon1);
    float y = sinf(dlon) * cosf(radians(lat2));
    float x = cosf(radians(lat1)) * sinf(radians(lat2)) -
              sinf(radians(lat1)) * cosf(radians(lat2)) * cosf(dlon);
    return fmodf(degrees(atan2f(y, x)) + 360.0f, 360.0f);
}

static void fmt_distance(float m, char *buf, size_t cap) {
    if (m < 1000.0f) snprintf(buf, cap, "%dm", (int)lroundf(m));
    else             snprintf(buf, cap, "%.2fkm", m / 1000.0f);
}

#if HAS_TFT
// Polished header: accent title (left), live GPS status cluster + page dots
// (right), a 2px accent rule, and a dim footer hint. Shared by every GPS page.
static void chrome_tft(const char *footer = nullptr, const char *title = nullptr) {
    T(6, 16, DISPLAY_CYAN, title ? title : page_title());

    // Status cluster: fix-quality dot + satellite count.
    bool fix = gps_has_fix(); int sats = gps_satellites();
    uint16_t fc = fix ? DISPLAY_GREEN : (sats > 0 ? DISPLAY_YELLOW : DISPLAY_RED);
    cv.fillCircle(168, 10, 4, fc);
    char sb[8]; snprintf(sb, sizeof(sb), "%d", sats);
    Ts(178, 6, DISPLAY_WHITE, sb);

    // Page position dots, far right.
    for (int i = 0; i < PAGE_COUNT; i++) {
        int x = 206 + i * 9;
        if (i == page) cv.fillCircle(x, 9, 3, DISPLAY_CYAN);
        else           cv.fillCircle(x, 9, 1, DISPLAY_GRAY);
    }

    cv.fillRect(0, 20, 240, 2, DISPLAY_CYAN); // accent rule
    char f[40];
    if (!footer) { footer_text(f, sizeof(f), false); footer = f; }
    Ts(6, 126, DISPLAY_GRAY, footer);
}

// Round compass rose into the canvas. `cardRot` rotates the whole card (0 =
// North up); `needleAngle` is the needle's on-screen angle. North-up mode uses
// cardRot=0 + needle=heading; track-up uses cardRot=-heading + needle=0 (up).
static void draw_rose(int cx, int cy, int r, float cardRot, float needleAngle, bool live, uint16_t ring, bool trackUp) {
    // Secondary rose elements are bright white in day themes, themed otherwise.
    uint16_t sec = theme_bright_ui() ? DISPLAY_WHITE : DISPLAY_GRAY;
    cv.drawCircle(cx, cy, r, sec);                // outer bezel
    cv.drawCircle(cx, cy, r - 3, ring);           // inner ring
    for (int a = 0; a < 360; a += 30) {           // degree ticks (rotate with card)
        bool card = (a % 90 == 0);
        int x0, y0, x1, y1;
        polar(cx, cy, r - 4, a + cardRot, &x0, &y0);
        polar(cx, cy, r - (card ? 12 : 7), a + cardRot, &x1, &y1);
        cv.drawLine(x0, y0, x1, y1, card ? DISPLAY_WHITE : sec);
    }
    static const char *CN[4] = { "N", "E", "S", "W" };
    for (int i = 0; i < 4; i++) {                 // cardinals ride the card (N stays red)
        int lx, ly; polar(cx, cy, r - 15, i * 90 + cardRot, &lx, &ly);
        T(lx - 5, ly + 5, i == 0 ? DISPLAY_RED : sec, CN[i]);
    }
    if (trackUp) {                                // track-up: bold fixed up-arrow (always)
        draw_navarrow(cv, cx, cy, r - 12, DISPLAY_RED); // inset so it clears the bezel
    } else if (live) {                            // north-up: two-tone needle (kite)
        int ntx, nty, stx, sty, lx, ly, rx, ry;
        polar(cx, cy, r - 14, needleAngle,       &ntx, &nty);
        polar(cx, cy, r - 14, needleAngle + 180, &stx, &sty);
        polar(cx, cy, 6,      needleAngle + 90,  &rx, &ry);
        polar(cx, cy, 6,      needleAngle - 90,  &lx, &ly);
        cv.fillTriangle(ntx, nty, lx, ly, rx, ry, DISPLAY_RED);
        cv.fillTriangle(stx, sty, lx, ly, rx, ry, sec);
        cv.fillCircle(cx, cy, 4, DISPLAY_WHITE);
    } else {
        cv.fillCircle(cx, cy, 4, sec);
    }
}

// Automotive-style "bubble" ribbon: large bold N/NE/E/... labels (and dots
// between) drift past a fixed centre marker as you turn. `heading` in degrees.
static void draw_ribbon(int cxc, int yMid, int halfW, float heading, bool live) {
    const float ppd = halfW / 60.0f;              // px per degree (~+/-60 across)
    uint16_t sec = theme_bright_ui() ? DISPLAY_WHITE : DISPLAY_GRAY;
    uint16_t col = live ? DISPLAY_WHITE : sec;
    int baseY = yMid + 8, ruleY = yMid + 13;
    cv.drawFastHLine(cxc - halfW, ruleY, halfW * 2, sec);
    for (int d = 0; d < 360; d += 15) {           // minor dots
        if (d % 45 == 0) continue;
        int x = cxc + (int)lroundf(wrap180(d - heading) * ppd);
        if (x > cxc - halfW && x < cxc + halfW) cv.fillCircle(x, ruleY, 1, sec);
    }
    static const char *C8[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    cv.setFont(&FreeSansBold12pt7b);
    cv.setTextSize(1);
    for (int i = 0; i < 8; i++) {                 // big bold labels
        int x = cxc + (int)lroundf(wrap180(i * 45 - heading) * ppd);
        if (x < cxc - halfW + 14 || x > cxc + halfW - 14) continue;
        int16_t x1, y1; uint16_t w, h;
        cv.getTextBounds(C8[i], 0, 0, &x1, &y1, &w, &h);
        cv.setTextColor(i == 0 ? DISPLAY_RED : col);
        cv.setCursor(x - w / 2, baseY);
        cv.print(C8[i]);
    }
    cv.setFont(&FreeSans9pt7b);
    cv.fillTriangle(cxc - 6, yMid - 16, cxc + 6, yMid - 16, cxc, yMid - 6, DISPLAY_RED); // marker
}
#endif

// =============================================================================
//  Page 1: Fix / Sky
// =============================================================================

static void draw_fix() {
    bool fix   = gps_has_fix();
    int  sats  = gps_satellites();
    float hdop = gps_hdop();
    float lat  = gps_latitude(), lon = gps_longitude(), alt = gps_altitude();
    int hh = 0, mm = 0, ss = 0;
    gps_datetime(nullptr, nullptr, nullptr, &hh, &mm, &ss);
    bool tvalid = gps_time_valid();

    const char* fixstr = fix ? "3D" : (sats > 0 ? "2D" : "--");
    uint16_t fixcol = fix ? DISPLAY_GREEN : (sats > 0 ? DISPLAY_YELLOW : DISPLAY_RED);
    uint16_t satcol = sats >= 6 ? DISPLAY_GREEN : (sats >= 4 ? DISPLAY_YELLOW : DISPLAY_RED);
    int satpct = sats > 12 ? 100 : (sats * 100) / 12;

    char hdopbuf[12], satbuf[12], llbuf[28], altbuf[16], timebuf[12];
    snprintf(hdopbuf, sizeof(hdopbuf), hdop > 0 ? "%.1f" : "--", hdop);
    snprintf(satbuf, sizeof(satbuf), "Sats %d", sats);
    snprintf(llbuf, sizeof(llbuf), "%.4f, %.4f", lat, lon);
    snprintf(altbuf, sizeof(altbuf), "Alt %dm", (int)alt);
    if (tvalid) snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02dZ", hh, mm, ss);
    else        snprintf(timebuf, sizeof(timebuf), "--:--Z");

#if HAS_OLED
    display_draw_text_small_abs(0, 0, DISPLAY_CYAN, "Fix/Sky");
    display_draw_text_small_abs(108, 0, DISPLAY_CYAN, "1/4");
    display_draw_hline(0, 9, 128, DISPLAY_GRAY);
    char r1[20]; snprintf(r1, sizeof(r1), "Fix %s  HDOP %s", fixstr, hdopbuf);
    display_draw_text_small_abs(0, 13, fixcol, r1);
    display_draw_text_small_abs(0, 23, satcol, satbuf);
    { int bx = 48, by = 23, bw = 78, bh = 7;
      tft.drawRect(bx, by, bw, bh, DISPLAY_WHITE);
      int fw = ((bw - 2) * satpct) / 100; if (fw > 0) tft.fillRect(bx + 1, by + 1, fw, bh - 2, satcol); }
    display_draw_text_small_abs(0, 33, DISPLAY_WHITE, llbuf);
    display_draw_text_small_abs(0, 43, DISPLAY_WHITE, altbuf);
    display_draw_text_small_abs(62, 43, DISPLAY_CYAN, timebuf);
    char f[20]; footer_text(f, sizeof(f), true);
    display_draw_text_small_abs(0, 55, DISPLAY_CYAN, f);
#else
    chrome_tft();
    char r1a[10]; snprintf(r1a, sizeof(r1a), "Fix %s", fixstr);
    T(8, 46, fixcol, r1a);
    T(110, 46, dop_color(hdop), "HDOP");
    T(178, 46, dop_color(hdop), hdopbuf);
    T(8, 70, satcol, satbuf);
    { int bx = 96, by = 58, bw = 136, bh = 14;
      cv.drawRect(bx, by, bw, bh, DISPLAY_WHITE);
      int fw = ((bw - 2) * satpct) / 100; if (fw > 0) cv.fillRect(bx + 1, by + 1, fw, bh - 2, satcol); }
    T(8, 94, DISPLAY_WHITE, llbuf);
    T(8, 116, DISPLAY_WHITE, altbuf);
    T(120, 116, DISPLAY_CYAN, timebuf);
#endif
}

// =============================================================================
//  Page 2: Compass
// =============================================================================

static void draw_compass() {
    float course = gps_course_deg();
    float spd    = gps_speed_kmh();
    bool moving  = motion_state(spd, gps_course_valid());
    static float s_lastHeading = 0.0f;
    float shown;
    if (moving) { shown = needle_step(s_compassN, course); s_lastHeading = shown; }
    else        shown = s_lastHeading; // no data: hold the last known heading
    char hdg[8], spdbuf[12];
    if (moving) snprintf(hdg, sizeof(hdg), "%03d", ((int)lroundf(shown)) % 360);
    else        snprintf(hdg, sizeof(hdg), "---"); // stopped: the hub dot says it, no imperative
    snprintf(spdbuf, sizeof(spdbuf), "%.1f km/h", spd);
    // Bright white in day themes; dim only the stationary state in Night/Amber.
    uint16_t ring = (moving || theme_bright_ui()) ? DISPLAY_WHITE : DISPLAY_GRAY;

    // Track-up rotates the card so the heading is up (held when stopped); bubble
    // is a flat ribbon. North-up keeps the card fixed and turns the needle.
    float cardRot = (compassMode == CM_DIR_UP) ? -shown : 0.0f;
    float needleA = (compassMode == CM_DIR_UP) ? 0.0f : shown;

#if HAS_OLED
    display_draw_text_small_abs(0, 0, DISPLAY_CYAN, "Compass");
    display_draw_text_small_abs(108, 0, DISPLAY_CYAN, "2/4");
    display_draw_hline(0, 9, 128, DISPLAY_GRAY);
    uint16_t needle = moving ? DISPLAY_RED : DISPLAY_GRAY;
    if (compassMode == CM_BUBBLE) {
        const int cxc = 64, yb = 13; const float ppd = 56.0f / 55.0f;
        display_draw_hline(cxc - 56, yb + 11, 112, DISPLAY_WHITE);
        for (int d = 0; d < 360; d += 15) {
            if (d % 45 == 0) continue;
            int x = cxc + (int)lroundf(wrap180(d - shown) * ppd);
            if (x > cxc - 56 && x < cxc + 56) tft.drawPixel(x, yb + 11, DISPLAY_WHITE);
        }
        static const char *C8[8] = { "N","NE","E","SE","S","SW","W","NW" };
        for (int i = 0; i < 8; i++) {
            int x = cxc + (int)lroundf(wrap180(i * 45 - shown) * ppd);
            if (x < cxc - 52 || x > cxc + 52) continue;
            int w = strlen(C8[i]) * 6;
            display_draw_text_small_abs(x - w / 2, yb, DISPLAY_WHITE, C8[i]);
        }
        display_draw_vline(cxc, yb - 3, 6, DISPLAY_WHITE); // centre marker
    } else {
        const int cx = 21, cy = 32, r = 12;
        tft.drawCircle(cx, cy, r, ring);
        static const char *CN[4] = { "N","E","S","W" };
        for (int i = 0; i < 4; i++) {
            int lx, ly; polar(cx, cy, r + 4, i * 90 + cardRot, &lx, &ly);
            display_draw_text_small_abs(lx - 2, ly - 3, DISPLAY_WHITE, CN[i]);
        }
        if (compassMode == CM_DIR_UP) draw_navarrow(tft, cx, cy, r - 2, DISPLAY_WHITE); // always
        else if (moving)              draw_arrow(tft, cx, cy, r - 2, needleA, needle);
        else                          tft.fillCircle(cx, cy, 2, DISPLAY_WHITE);
    }
    char hb[14]; snprintf(hb, sizeof(hb), "%s %s", hdg, moving ? cardinal8(shown) : "");
    display_draw_text_small_abs(46, 30, DISPLAY_WHITE, hb);
    display_draw_text_small_abs(46, 42, DISPLAY_WHITE, spdbuf);
    char f[22]; snprintf(f, sizeof(f), "S:Pg D:%s L:Bk", compass_mode_name(compassMode));
    display_draw_text_small_abs(0, 55, DISPLAY_CYAN, f);
#else
    // Mode is conveyed by the centre symbol (needle / up-arrow / ribbon), so no
    // text label is needed -- gives the rose and readout their full space back.
    chrome_tft("Short:Page  Dbl:Mode  Long:Back");
    if (compassMode == CM_BUBBLE) {
        draw_ribbon(120, 58, 112, shown, moving);
        T(78, 110, moving ? DISPLAY_GREEN : DISPLAY_GRAY, hdg, 2);
        if (moving) T(150, 102, DISPLAY_WHITE, cardinal8(shown));
        Ts(8, 118, DISPLAY_GRAY, spdbuf);
    } else {
        draw_rose(60, 74, 44, cardRot, needleA, moving, ring, compassMode == CM_DIR_UP);
        Ts(120, 38, DISPLAY_GRAY, "HEADING");
        T(120, 74, moving ? DISPLAY_GREEN : DISPLAY_GRAY, hdg, 2);
        if (moving) T(190, 66, DISPLAY_WHITE, cardinal8(shown));
        Ts(120, 96, DISPLAY_GRAY, "SPEED");
        T(120, 118, DISPLAY_WHITE, spdbuf);
    }
#endif
}

// =============================================================================
//  Page 3: Wayfinder
// =============================================================================

static void draw_wayfinder() {
    const Waypoint *w = nav_target();
    if (!w) {
        bool have = wp_count() > 0;
#if HAS_OLED
        display_draw_text_small_abs(0, 0, DISPLAY_CYAN, "Wayfinder");
        display_draw_text_small_abs(108, 0, DISPLAY_CYAN, "3/4");
        display_draw_hline(0, 9, 128, DISPLAY_GRAY);
        display_draw_text_small_abs(0, 22, DISPLAY_WHITE, have ? "No target set." : "No waypoints.");
        display_draw_text_small_abs(0, 34, DISPLAY_WHITE, have ? "Dbl: cycle target" : "Mark some first.");
        display_draw_text_small_abs(0, 55, DISPLAY_CYAN, "S:Pg D:Tgt L:Back");
#else
        chrome_tft("Short:Page  Dbl:Target  Long:Back");
        T(8, 64, DISPLAY_WHITE, have ? "No target set." : "No waypoints stored.");
        T(8, 92, DISPLAY_WHITE, have ? "Double-press = target" : "Mark some first.");
#endif
        return;
    }

    bool route   = (nav_mode() == NAV_ROUTE);
    bool fix     = gps_has_fix();
    float course = gps_course_deg();
    float spd    = gps_speed_kmh();
    bool moving  = motion_state(spd, gps_course_valid());
    float dist = 0, brg = 0;
    if (fix) {
        dist = haversine_m(gps_latitude(), gps_longitude(), w->lat, w->lon);
        brg  = bearing_deg(gps_latitude(), gps_longitude(), w->lat, w->lon);
    }
    bool arrived = nav_arrived_final();
    float arrow = moving ? fmodf(brg - course + 360.0f, 360.0f) : brg;
    float shownArrow = needle_step(s_wayfindN, arrow); // inertial swing, like the compass
    uint16_t acol = arrived ? DISPLAY_GREEN : (moving ? DISPLAY_CYAN : DISPLAY_GRAY);

    char distbuf[12], brgbuf[8], etabuf[8], tgt[20];
    fmt_distance(dist, distbuf, sizeof(distbuf));
    snprintf(brgbuf, sizeof(brgbuf), "%03d", ((int)lroundf(brg)) % 360);
    if (route) snprintf(tgt, sizeof(tgt), "%s L%d/%d", w->name, nav_route_leg(), nav_route_total());
    else       snprintf(tgt, sizeof(tgt), ">%s", w->name);
    if (moving && spd > 0.5f && !arrived)
        snprintf(etabuf, sizeof(etabuf), "%dm", (int)lroundf((dist / (spd / 3.6f)) / 60.0f));
    else
        snprintf(etabuf, sizeof(etabuf), "--");

#if HAS_OLED
    // Bands: title 0-8, divider @9, content 11-53, footer @55.
    display_draw_text_small_abs(0, 0, DISPLAY_CYAN, tgt);
    display_draw_text_small_abs(108, 0, DISPLAY_CYAN, "3/4");
    display_draw_hline(0, 9, 128, DISPLAY_GRAY);
    const int cx = 21, cy = 32, r = 12;           // circle y20-44, inside content band
    tft.drawCircle(cx, cy, r, DISPLAY_WHITE);
    if (!fix)         display_draw_text_small_abs(cx - 6, cy - 3, DISPLAY_WHITE, "NF");
    else if (arrived) display_draw_text_small_abs(cx - 6, cy - 3, DISPLAY_GREEN, "HR");
    else              draw_arrow(tft, cx, cy, r - 2, shownArrow, acol);
    char rb[16];
    display_draw_text_small_abs(44, 16, DISPLAY_WHITE, distbuf);
    snprintf(rb, sizeof(rb), "BRG %s", brgbuf); display_draw_text_small_abs(44, 28, DISPLAY_WHITE, rb);
    snprintf(rb, sizeof(rb), "ETA %s", etabuf); display_draw_text_small_abs(44, 40, DISPLAY_WHITE, rb);
    display_draw_text_small_abs(0, 55, DISPLAY_CYAN, "S:Pg D:Tgt L:Back");
#else
    // Target name becomes the title (avoids colliding with the page dots).
    chrome_tft("Short:Page  Dbl:Target  Long:Back", tgt);
    const int cx = 54, cy = 72, r = 28;           // circle y44-100, inside content band
    cv.drawCircle(cx, cy, r, DISPLAY_WHITE);
    if (!fix)         T(cx - 22, cy + 5, DISPLAY_WHITE, "NoFix");
    else if (arrived) T(cx - 22, cy + 5, DISPLAY_GREEN, "HERE");
    else              draw_arrow(cv, cx, cy, r - 4, shownArrow, acol);
    Ts(116, 38, DISPLAY_GRAY, "DIST");
    T(116, 64, arrived ? DISPLAY_GREEN : DISPLAY_WHITE, distbuf);
    Ts(116, 82, DISPLAY_GRAY, "BRG");
    T(116, 106, DISPLAY_WHITE, brgbuf);
    Ts(178, 82, DISPLAY_GRAY, "ETA");
    T(178, 106, DISPLAY_WHITE, etabuf);
#endif
}

// =============================================================================
//  Page 4: Waypoint Manager (list + context menu)
// =============================================================================

// One waypoint row "WPT01  123m  045" with distance/bearing if we have a fix.
static void wp_row_text(int idx, char *buf, size_t cap) {
    const Waypoint *w = wp_get(idx);
    if (!w) { buf[0] = '\0'; return; }
    if (gps_has_fix()) {
        float d = haversine_m(gps_latitude(), gps_longitude(), w->lat, w->lon);
        float b = bearing_deg(gps_latitude(), gps_longitude(), w->lat, w->lon);
        char db[10]; fmt_distance(d, db, sizeof(db));
        snprintf(buf, cap, "%-7s %7s %03d", w->name, db, ((int)lroundf(b)) % 360);
    } else {
        snprintf(buf, cap, "%-7s %.4f,%.4f", w->name, w->lat, w->lon);
    }
}

static void draw_wp_list() {
    int n = wp_count();
    clamp_wp_cursor();
    char hdr[22];
    snprintf(hdr, sizeof(hdr), "Waypoints %d/%d", n, WP_MAX);

#if HAS_OLED
    display_draw_text_small_abs(0, 0, DISPLAY_CYAN, hdr);
    display_draw_text_small_abs(108, 0, DISPLAY_CYAN, "4/4");
    display_draw_hline(0, 9, 128, DISPLAY_GRAY);
    if (n == 0) {
        display_draw_text_small_abs(0, 24, DISPLAY_WHITE, "(empty) Dbl opens");
        display_draw_text_small_abs(0, 34, DISPLAY_WHITE, "menu: Mark Here");
    } else {
        const int ROWS = 4;
        int top = wpCursor - ROWS / 2; if (top > n - ROWS) top = n - ROWS; if (top < 0) top = 0;
        for (int r = 0; r < ROWS && top + r < n; r++) {
            int idx = top + r, y = 12 + r * 10;
            char row[28]; wp_row_text(idx, row, sizeof(row));
            if (idx == wpCursor) {
                display_fill_rect_abs(0, y - 1, 128, 9, DISPLAY_CYAN);
                display_draw_text_small_abs(2, y, DISPLAY_BLACK, row);
            } else {
                display_draw_text_small_abs(2, y, DISPLAY_WHITE, row);
            }
        }
    }
    display_draw_text_small_abs(0, 55, DISPLAY_CYAN, "S:Move D:Menu L:Back");
#else
    chrome_tft("Short:Move  Dbl:Menu  Long:Back", hdr);
    if (n == 0) {
        T(8, 64, DISPLAY_WHITE, "(empty)");
        T(8, 92, DISPLAY_GRAY, "Dbl: menu -> Mark Here");
    } else {
        const int ROWS = 4;
        int top = wpCursor - ROWS / 2; if (top > n - ROWS) top = n - ROWS; if (top < 0) top = 0;
        for (int r = 0; r < ROWS && top + r < n; r++) {
            int idx = top + r, y = 40 + r * 22;
            char row[28]; wp_row_text(idx, row, sizeof(row));
            if (idx == wpCursor) {
                cv.fillRect(0, y - 16, 240, 21, DISPLAY_CYAN);
                T(6, y, DISPLAY_BLACK, row);
            } else {
                T(6, y, DISPLAY_WHITE, row);
            }
        }
    }
#endif
}

static void draw_wp_menu() {
    const Waypoint *w = wp_get(wpCursor);
    char title[20];
    snprintf(title, sizeof(title), "WP: %s", w ? w->name : "(none)");

#if HAS_OLED
    display_draw_text_small_abs(0, 0, DISPLAY_CYAN, title);
    display_draw_hline(0, 9, 128, DISPLAY_GRAY);
    const int ROWS = 4;
    int top = wpMenuSel - ROWS / 2;
    if (top > WP_MENU_COUNT - ROWS) top = WP_MENU_COUNT - ROWS; if (top < 0) top = 0;
    for (int r = 0; r < ROWS && top + r < WP_MENU_COUNT; r++) {
        int idx = top + r, y = 12 + r * 10;
        if (idx == wpMenuSel) {
            display_fill_rect_abs(0, y - 1, 128, 9, DISPLAY_CYAN);
            display_draw_text_small_abs(2, y, DISPLAY_BLACK, WP_MENU_ITEMS[idx]);
        } else {
            display_draw_text_small_abs(2, y, DISPLAY_WHITE, WP_MENU_ITEMS[idx]);
        }
    }
    display_draw_text_small_abs(0, 55, DISPLAY_CYAN, "S:Item D:Sel L:Cancel");
#else
    chrome_tft("Short:Item  Dbl:Select  Long:Cancel", title);
    const int ROWS = 4;
    int top = wpMenuSel - ROWS / 2;
    if (top > WP_MENU_COUNT - ROWS) top = WP_MENU_COUNT - ROWS; if (top < 0) top = 0;
    for (int r = 0; r < ROWS && top + r < WP_MENU_COUNT; r++) {
        int idx = top + r, y = 30 + r * 22;
        if (idx == wpMenuSel) {
            cv.fillRect(0, y - 16, 240, 21, DISPLAY_CYAN);
            T(6, y, DISPLAY_BLACK, WP_MENU_ITEMS[idx]);
        } else {
            T(6, y, DISPLAY_WHITE, WP_MENU_ITEMS[idx]);
        }
    }
#endif
}

static void draw_waypoints() {
    if (wpState == WP_MENU) draw_wp_menu();
    else                    draw_wp_list();
}

// Renders a brief centered status message (used during averaging).
static void render_message(const char *l1, const char *l2, const char *l3 = nullptr) {
#if HAS_OLED
    display_clear();
    display_draw_text_small_abs(8, 24, DISPLAY_CYAN, l1);
    if (l2) display_draw_text_small_abs(8, 36, DISPLAY_WHITE, l2);
    if (l3) display_draw_text_small_abs(8, 55, DISPLAY_GRAY, l3);
    display_update_buffer();
#else
    cv.fillScreen(DISPLAY_BLACK);
    T(20, 60, DISPLAY_CYAN, l1);
    if (l2) T(20, 90, DISPLAY_WHITE, l2);
    if (l3) Ts(8, 126, DISPLAY_GRAY, l3);
    tft.drawRGBBitmap(0, 0, cv.getBuffer(), 240, 135);
#endif
}

// Averages up to 30 fixes over ~6s for a more accurate mark. Blocking, but
// polls the button so a double-press aborts. Returns the new waypoint index,
// or -1 if no fix was seen or the user cancelled.
static int do_average() {
    double sl = 0, so = 0, sa = 0;
    int n = 0;
    bool cancelled = false;
    uint32_t start = millis();
    while (!cancelled && millis() - start < 6000 && n < 30) {
        gps_update();
        if (gps_has_fix()) {
            sl += gps_latitude(); so += gps_longitude(); sa += gps_altitude(); n++;
            char msg[24]; snprintf(msg, sizeof(msg), "Averaging %d/30", n);
            render_message("Hold still...", msg, "Dbl-press = cancel");
        } else {
            render_message("Averaging...", "waiting for fix", "Dbl-press = cancel");
        }
        // Space the samples out, but keep polling the button so a deliberate
        // double-press can abort (the main loop is blocked while we're here).
        uint32_t t = millis();
        while (millis() - t < 180) {
            button_update();
            if (button_double_pressed()) { cancelled = true; break; }
            delay(10);
        }
    }
    if (cancelled) {
        render_message("Averaging", "cancelled", nullptr);
        delay(700);
        return -1;
    }
    if (n == 0) return -1;
    return wp_add((float)(sl / n), (float)(so / n), (float)(sa / n));
}

// Executes the highlighted context-menu action.
static void wp_menu_execute(int sel) {
    switch (sel) {
        case 0: // Go To
            if (wp_get(wpCursor)) { nav_goto(wpCursor); page = PAGE_WAYFINDER; }
            break;
        case 1: // Start Route (from cursor to end)
            nav_route_start(wpCursor); page = PAGE_WAYFINDER;
            break;
        case 2: // Mark Here
            if (gps_has_fix()) { int i = wp_add(gps_latitude(), gps_longitude(), gps_altitude());
                                 if (i >= 0) wpCursor = i; }
            break;
        case 3: { // Average Here
            int i = do_average(); if (i >= 0) wpCursor = i;
            break; }
        case 4: // Move Up
            if (wp_move_up(wpCursor)) wpCursor--;
            break;
        case 5: // Move Down
            if (wp_move_down(wpCursor)) wpCursor++;
            break;
        case 6: // Delete
            wp_delete(wpCursor); clamp_wp_cursor();
            break;
        case 7: // Sort Nearest
            if (gps_has_fix()) { wp_sort_nearest(gps_latitude(), gps_longitude()); wpCursor = 0; }
            break;
        case 8: // Sort Name
            wp_sort_name(); wpCursor = 0;
            break;
        case 9: // Stop Nav
            nav_stop();
            break;
        case 10: // Back
        default:
            break;
    }
    wpState = WP_LIST;
}

// ---- frame driver -----------------------------------------------------------

void gpsview_update() {
    uint32_t now = millis();
    // Animate the needle pages fast while a needle is still settling, idle slow
    // otherwise to save power and SPI bandwidth.
    uint32_t interval = 250;
    if (page == PAGE_COMPASS || page == PAGE_WAYFINDER)
        interval = s_needleAnimating ? 70 : 250;
    if (!needClear && now - lastDraw < interval) return;
    lastDraw = now;
    needClear = false;
    s_needleAnimating = false; // re-set by needle_step during this frame's draw

#if HAS_OLED
    display_clear();
#else
    cv.fillScreen(DISPLAY_BLACK);
#endif

    switch (page) {
        case PAGE_FIX:       draw_fix(); break;
        case PAGE_COMPASS:   draw_compass(); break;
        case PAGE_WAYFINDER: draw_wayfinder(); break;
        case PAGE_WAYPOINTS: draw_waypoints(); break;
        default: break;
    }

#if HAS_OLED
    display_update_buffer();
#else
    tft.drawRGBBitmap(0, 0, cv.getBuffer(), 240, 135);
#endif
}
