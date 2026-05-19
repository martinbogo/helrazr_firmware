#include "button.h"
#include "pins.h"

static const uint32_t DEBOUNCE_MS = 50;

static bool     lastRaw      = HIGH;
static bool     debounced    = HIGH;
static uint32_t pressStart   = 0;
static uint32_t lastChangeMs = 0;

static bool eventShort    = false;
static bool eventLong     = false;
static bool eventPowerOff = false;

static bool longFired     = false;  // 600ms event fired this press
static bool powerFired    = false;  // 10s event fired this press

void button_init() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
}

void button_update() {
    eventShort    = false;
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
        // Release -- fire short press if not already handled by a hold event
        if (!longFired && !powerFired) {
            eventShort = true;
        }
        // longFired / powerFired reset on next press start

    } else if (debounced == LOW) {
        // Still held -- fire timed events
        uint32_t held = now - pressStart;

        if (!longFired && held >= BTN_LONG_MS) {
            eventLong  = true;
            longFired  = true;
        }
        if (!powerFired && held >= BTN_POWEROFF_MS) {
            eventPowerOff = true;
            powerFired    = true;
        }
    }
}

bool     button_short_pressed()    { return eventShort; }
bool     button_long_pressed()     { return eventLong; }
bool     button_poweroff_pressed() { return eventPowerOff; }
uint32_t button_held_ms() {
    if (debounced == HIGH) return 0;
    return millis() - pressStart;
}
