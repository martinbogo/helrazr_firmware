/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "button.h"
#include "pins.h"

// ---------------------------------------------------------------------------
// Single-paradigm debounced poller. No interrupt.
//
// Earlier revisions used an edge ISR to "rescue" fast taps, then spent their
// complexity reconciling that ISR-captured state against the polled pin level.
// Every regression (latch-until-reboot, flicker-on-hold, phantom presses that
// drained after a pause) lived in that seam: two debounce mechanisms with
// different time constants, shared volatiles touched from both the handler and
// the loop without a critical section, and millis()/digitalRead() called from
// GPIOTE context on the nRF52. The ISR bought almost nothing -- a real human
// tap lasts tens of milliseconds and spans several polls even during a slow
// TFT blit, and the one genuinely blocking operation (GPS position averaging)
// already polls this routine itself.
//
// So there is now a single source of truth: a time-debounced pin level sampled
// in button_update(). A glitch shorter than DEBOUNCE_MS can never flip the
// level, which rejects both contact chatter and electrical noise for free. The
// level always tracks the real pin, so the machine cannot latch, desync, or
// accumulate phantom events.
// ---------------------------------------------------------------------------

static const uint32_t DEBOUNCE_MS = 15;    // stable time before a level is accepted
static const uint32_t DOUBLE_MS   = 400;   // max gap between releases for a double

// Debounced physical level
static int      rawLast      = HIGH;
static uint32_t rawChangeMs  = 0;
static bool     pressed      = false;      // accepted, debounced pressed state
static uint32_t pressStartMs = 0;

// Click classification
static int      clickCount    = 0;
static uint32_t lastReleaseMs = 0;

// Per-press latches
static bool longFired          = false;
static bool powerFired         = false;
static bool suppressClick      = false;    // a long/power press eats its own release
static bool ignoreUntilRelease = false;

// Output events (true for one button_update() cycle)
static bool eventShort    = false;
static bool eventDouble   = false;
static bool eventLong     = false;
static bool eventPowerOff = false;

static uint32_t lastActivityMs = 0;

void button_init() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    rawLast      = digitalRead(PIN_BUTTON);
    rawChangeMs  = millis();
    pressed      = (rawLast == LOW);
    lastActivityMs = millis();
}

// Register one completed click into the short/double classifier.
static void register_click(uint32_t now) {
    if (clickCount == 1 && (now - lastReleaseMs) < DOUBLE_MS) {
        eventDouble   = true;
        clickCount    = 0;
        lastReleaseMs = 0;
    } else {
        clickCount    = 1;
        lastReleaseMs = now;
    }
}

void button_update() {
    eventShort    = false;
    eventDouble   = false;
    eventLong     = false;
    eventPowerOff = false;

    uint32_t now = millis();

    // --- Debounce: accept a level only after it has been stable DEBOUNCE_MS ---
    int raw = digitalRead(PIN_BUTTON);
    if (raw != rawLast) {
        rawLast     = raw;
        rawChangeMs = now;
    }
    bool level = pressed;
    if (now - rawChangeMs >= DEBOUNCE_MS) {
        level = (raw == LOW);
    }

    // --- Press begins ---
    if (level && !pressed) {
        pressed       = true;
        pressStartMs  = now;
        longFired     = false;
        powerFired    = false;
        suppressClick = false;
        lastActivityMs = now;
    }

    // --- Press ends ---
    if (!level && pressed) {
        pressed = false;
        lastActivityMs = now;
        if (ignoreUntilRelease) {
            ignoreUntilRelease = false;
        } else if (!suppressClick) {
            register_click(now);
        }
    }

    // --- Held-button events ---
    if (pressed) {
        lastActivityMs = now;
        if (!ignoreUntilRelease) {
            uint32_t held = now - pressStartMs;
            if (!longFired && held >= BTN_LONG_MS) {
                eventLong     = true;
                longFired     = true;
                suppressClick = true;    // don't emit a short on release
                clickCount    = 0;       // cancel any pending short
                lastActivityMs = now;
            }
            if (!powerFired && held >= BTN_POWEROFF_MS) {
                eventPowerOff = true;
                powerFired    = true;
                lastActivityMs = now;
            }
        }
    } else {
        // Pending single click resolves once the double window expires.
        if (clickCount > 0 && (now - lastReleaseMs) >= DOUBLE_MS) {
            eventShort = true;
            clickCount = 0;
        }
    }
}

bool     button_short_pressed()    { return eventShort; }
bool     button_double_pressed()   { return eventDouble; }
bool     button_long_pressed()     { return eventLong; }
bool     button_poweroff_pressed() { return eventPowerOff; }
uint32_t button_held_ms() {
    if (!pressed) return 0;
    return millis() - pressStartMs;
}

uint32_t button_last_activity_ms() {
    return lastActivityMs;
}

void button_consume() {
    clickCount    = 0;
    eventShort    = false;
    eventDouble   = false;
    eventLong     = false;
    eventPowerOff = false;
    if (pressed) ignoreUntilRelease = true;
}
