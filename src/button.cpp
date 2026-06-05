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
// Interrupt-captured edges + self-correcting reconciliation.
//
// The ISR is what makes the button responsive: on the t114 a full-screen TFT
// blit makes a single loop iteration tens of milliseconds long, so a quick tap
// can begin and end entirely between two button_update() polls. Edge
// interrupts catch that press regardless of how busy the render loop is.
//
// The old failure mode was a *latch*: the ISR's blanket 30 ms debounce could
// swallow a release edge under vibration, leaving isrDown stuck true with no
// recovery short of a reboot. We now (a) shrink the ISR debounce so it is far
// less likely to eat a real transition, and (b) reconcile isrDown against the
// actual, separately-debounced pin level in button_update(): if the firmware
// believes the button is held but the pin has plainly been released for a
// while, we heal the state and register the missed click. A dropped edge now
// self-corrects in tens of milliseconds instead of requiring a power cycle.
// ---------------------------------------------------------------------------

static const uint32_t ISR_DEBOUNCE_MS = 8;     // per-edge bounce reject in the ISR
static const uint32_t RECONCILE_MS    = 40;    // pin-vs-ISR disagreement before healing
static const uint32_t DOUBLE_MS       = 400;   // max gap between releases for a double

// ISR-captured state
static volatile uint32_t isrPressStart  = 0;
static volatile uint32_t isrLastEdge    = 0;
static volatile uint32_t isrLastRelease = 0;
static volatile bool     isrDown        = false;
static volatile uint8_t  isrClickCount  = 0;

#if defined(ESP32)
static void IRAM_ATTR button_isr() {
#else
static void button_isr() {
#endif
    uint32_t now = millis();
    if (now - isrLastEdge < ISR_DEBOUNCE_MS) return;
    isrLastEdge = now;

    bool down = (digitalRead(PIN_BUTTON) == LOW);
    if (down) {
        if (!isrDown) {
            isrDown = true;
            isrPressStart = now;
        }
    } else {
        if (isrDown) {
            isrDown = false;
            isrClickCount++;
            isrLastRelease = now;
        }
    }
}

// Debounced physical level, tracked in button_update() for reconciliation
static int      rawLast      = HIGH;
static uint32_t rawChangeMs  = 0;
static bool     physDown     = false;

// Click classification
static uint32_t lastReleaseMs = 0;
static int      clickCount    = 0;

// Per-press latches
static bool longFired          = false;
static bool powerFired         = false;
static bool ignoreUntilRelease = false;

// Output events
static bool eventShort    = false;
static bool eventDouble   = false;
static bool eventLong     = false;
static bool eventPowerOff = false;

static uint32_t lastActivityMs = 0;

void button_init() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    rawLast     = digitalRead(PIN_BUTTON);
    rawChangeMs = millis();
    physDown    = (rawLast == LOW);
    isrDown     = physDown;
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), button_isr, CHANGE);
    lastActivityMs = millis();
}

void button_update() {
    eventShort    = false;
    eventDouble   = false;
    eventLong     = false;
    eventPowerOff = false;

    uint32_t now = millis();

    // --- Track the debounced physical pin level ---
    int raw = digitalRead(PIN_BUTTON);
    if (raw != rawLast) {
        rawLast = raw;
        rawChangeMs = now;
    }
    if (now - rawChangeMs >= ISR_DEBOUNCE_MS) {
        physDown = (raw == LOW);
    }

    // --- Reconcile ISR state with reality (heals dropped edges) ---
    // Only act once the pin has been stable past RECONCILE_MS, by which point
    // the ISR is quiet, so there is no live race with the handler.
    if (now - rawChangeMs >= RECONCILE_MS) {
        if (isrDown && !physDown) {
            // A release edge was missed: register the click we never saw.
            isrDown = false;
            isrClickCount++;
            isrLastRelease = now;
        } else if (!isrDown && physDown) {
            // A press edge was missed.
            isrDown = true;
            isrPressStart = now;
        }
    }

    if (isrDown) {
        lastActivityMs = now;
    } else {
        ignoreUntilRelease = false;
    }

    // --- Consume completed clicks captured by the ISR ---
    while (isrClickCount > 0) {
        lastActivityMs = now;
        isrClickCount--;

        if (ignoreUntilRelease) continue;
        if (longFired || powerFired) continue;

        if (clickCount == 1 && (isrLastRelease - lastReleaseMs < DOUBLE_MS)) {
            eventDouble   = true;
            clickCount    = 0;
            lastReleaseMs = 0;
        } else {
            clickCount    = 1;
            lastReleaseMs = isrLastRelease;
        }
    }

    // --- Held-button events ---
    if (isrDown) {
        uint32_t held = now - isrPressStart;
        if (!longFired && held >= BTN_LONG_MS) {
            eventLong  = true;
            longFired  = true;
            clickCount = 0;          // cancel pending short
            lastActivityMs = now;
        }
        if (!powerFired && held >= BTN_POWEROFF_MS) {
            eventPowerOff = true;
            powerFired    = true;
            lastActivityMs = now;
        }
    } else {
        longFired  = false;
        powerFired = false;
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
    if (!isrDown) return 0;
    return millis() - isrPressStart;
}

uint32_t button_last_activity_ms() {
    return lastActivityMs;
}

void button_consume() {
    isrClickCount = 0;
    clickCount    = 0;
    eventShort    = false;
    eventDouble   = false;
    eventLong     = false;
    eventPowerOff = false;
    if (isrDown) ignoreUntilRelease = true;
}
