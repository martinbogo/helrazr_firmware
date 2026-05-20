#include "button.h"
#include "pins.h"

static const uint32_t DEBOUNCE_MS = 50;

static bool     lastRaw      = HIGH;
static bool     debounced    = HIGH;
static uint32_t pressStart   = 0;
static uint32_t lastChangeMs = 0;
static uint32_t lastReleaseMs = 0;
static int      clickCount   = 0;

static bool eventShort    = false;
static bool eventDouble   = false;
static bool eventLong     = false;
static bool eventPowerOff = false;

static bool longFired     = false;  // 600ms event fired this press
static bool powerFired    = false;  // 10s event fired this press

void button_init() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
}

void button_update() {
    eventShort    = false;
    eventDouble   = false;
    eventLong     = false;
    eventPowerOff = false;

    bool raw = digitalRead(PIN_BUTTON);
    uint32_t now = millis();

    if (raw != lastRaw) {
        lastChangeMs = now;
        lastRaw = raw;
    }
    if ((now - lastChangeMs) < DEBOUNCE_MS) return;

    bool prev = debounced;
    debounced = raw;

    if (prev == HIGH && debounced == LOW) {
        // Press start
        pressStart  = now;
        longFired   = false;
        powerFired  = false;

    } else if (prev == LOW && debounced == HIGH) {
        // Release -- handle double-click timing
        if (!longFired && !powerFired) {
            if (now - lastReleaseMs < 300) {
                // Second click in window
                eventDouble = true;
                clickCount = 0;
                lastReleaseMs = 0;
            } else {
                // First click
                clickCount = 1;
                lastReleaseMs = now;
            }
        } else {
            clickCount = 0;
        }

    } else if (debounced == LOW) {
        // Still held -- fire timed events
        uint32_t held = now - pressStart;

        if (!longFired && held >= BTN_LONG_MS) {
            eventLong  = true;
            longFired  = true;
            clickCount = 0;
        }
        if (!powerFired && held >= BTN_POWEROFF_MS) {
            eventPowerOff = true;
            powerFired    = true;
        }
    } else if (debounced == HIGH) {
        // Idle pending click resolution
        if (clickCount == 1 && (now - lastReleaseMs) >= 300) {
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
    if (debounced == HIGH) return 0;
    return millis() - pressStart;
}
