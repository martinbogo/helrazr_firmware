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
// Polled debounce state machine.
//
// The previous implementation derived button state from edge interrupts and a
// latched "pressed" flag. Any edge dropped by the ISR debounce (which happens
// readily under sustained vibration, e.g. a button rattling in a moving car)
// would desync that flag from the real pin level with no way to recover short
// of a reboot: a stuck-pressed flag fast-triggers long/power-off events and
// breaks double-press detection.
//
// Polling the pin every loop and debouncing in software is self-correcting:
// the debounced level always re-converges on the actual pin state, so no
// dropped transition can permanently strand the machine.
// ---------------------------------------------------------------------------

static const uint32_t DEBOUNCE_MS = 15;    // stable time before a level is accepted
static const uint32_t DOUBLE_MS   = 400;   // max gap between releases for a double

// Debounce tracking
static int      lastRaw        = HIGH;     // last raw pin reading
static uint32_t lastRawChange  = 0;        // when the raw reading last changed
static bool     pressed        = false;    // debounced pressed state (active low)
static uint32_t pressStartMs   = 0;        // when the current debounced press began

// Click accumulation for short/double detection
static int      pendingClicks  = 0;        // completed clicks awaiting classification
static uint32_t lastReleaseMs  = 0;        // debounced release time of last click

// Per-press latches
static bool     longFired      = false;
static bool     powerFired     = false;
static bool     suppressClick  = false;    // a long/power press eats its own release
static bool     ignoreUntilRelease = false;

// Output events (true for one button_update() cycle)
static bool eventShort    = false;
static bool eventDouble   = false;
static bool eventLong     = false;
static bool eventPowerOff = false;

static uint32_t lastActivityMs = 0;

void button_init() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    lastRaw       = digitalRead(PIN_BUTTON);
    lastRawChange = millis();
    pressed       = (lastRaw == LOW);
    lastActivityMs = millis();
}

void button_update() {
    eventShort    = false;
    eventDouble   = false;
    eventLong     = false;
    eventPowerOff = false;

    uint32_t now = millis();

    // --- Debounce: accept a level only after it has been stable DEBOUNCE_MS ---
    int raw = digitalRead(PIN_BUTTON);
    if (raw != lastRaw) {
        lastRaw = raw;
        lastRawChange = now;
    }

    bool debounced = pressed;
    if ((now - lastRawChange) >= DEBOUNCE_MS) {
        debounced = (raw == LOW);
    }

    // --- Edge handling on the debounced level (self-correcting) ---
    if (debounced && !pressed) {
        // Press edge
        pressed       = true;
        pressStartMs  = now;
        longFired     = false;
        powerFired    = false;
        suppressClick = false;
        lastActivityMs = now;
    } else if (!debounced && pressed) {
        // Release edge
        pressed = false;
        lastActivityMs = now;

        if (ignoreUntilRelease) {
            // The press that was active when button_consume() ran has ended.
            ignoreUntilRelease = false;
        } else if (!suppressClick) {
            // A normal click completed. Classify against any pending click.
            if (pendingClicks >= 1 && (now - lastReleaseMs) < DOUBLE_MS) {
                eventDouble  = true;
                pendingClicks = 0;
                lastReleaseMs = 0;
            } else {
                pendingClicks = 1;
                lastReleaseMs = now;
            }
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
                suppressClick = true;   // don't emit a short on release
                pendingClicks = 0;      // cancel any pending short
                lastActivityMs = now;
            }
            if (!powerFired && held >= BTN_POWEROFF_MS) {
                eventPowerOff = true;
                powerFired    = true;
                lastActivityMs = now;
            }
        }
    } else {
        // Pending single click resolves once the double-press window expires.
        if (pendingClicks > 0 && (now - lastReleaseMs) >= DOUBLE_MS) {
            eventShort   = true;
            pendingClicks = 0;
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
    pendingClicks = 0;
    eventShort    = false;
    eventDouble   = false;
    eventLong     = false;
    eventPowerOff = false;
    // If a press is currently active, swallow it (and its release) so the
    // consuming screen doesn't immediately receive a stale event.
    if (pressed) ignoreUntilRelease = true;
}
