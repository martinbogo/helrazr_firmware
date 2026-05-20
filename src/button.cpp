#include "button.h"
#include "pins.h"

// ISR variables
static volatile uint32_t isrPressStart = 0;
static volatile uint32_t isrLastEdge   = 0;
static volatile bool     isrPressed = false;
static volatile uint8_t  isrClickCount = 0;

#if defined(ESP32)
static void IRAM_ATTR button_isr() {
#else
static void button_isr() {
#endif
    bool state = digitalRead(PIN_BUTTON);
    uint32_t now = millis();
    
    // Ignore any bounce edges within 30ms of the last edge
    if (now - isrLastEdge < 30) return;
    isrLastEdge = now;
    
    if (state == LOW) {
        if (!isrPressed) {
            isrPressed = true;
            isrPressStart = now;
        }
    } else {
        if (isrPressed) {
            isrPressed = false;
            isrClickCount++;
        }
    }
}

static uint32_t lastReleaseMs = 0;
static int      clickCount   = 0;

static bool eventShort    = false;
static bool eventDouble   = false;
static bool eventLong     = false;
static bool eventPowerOff = false;

static bool longFired     = false;
static bool powerFired    = false;

void button_init() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), button_isr, CHANGE);
}

void button_update() {
    eventShort    = false;
    eventDouble   = false;
    eventLong     = false;
    eventPowerOff = false;

    uint32_t now = millis();

    // Consume clicks accumulated by the ISR
    while (isrClickCount > 0) {
        isrClickCount--;
        
        if (!longFired && !powerFired) {
            // A click has completely finished
            if (clickCount == 1 && (now - lastReleaseMs < 300)) {
                // If it's been less than 300ms since last release, it's a double
                eventDouble = true;
                clickCount = 0;
                lastReleaseMs = 0;
            } else {
                clickCount = 1;
                // We use now as the release time for next check
                lastReleaseMs = now; 
            }
        }
    }

    // Long press and idle logic
    if (isrPressed) {
        uint32_t held = now - isrPressStart;
        if (!longFired && held >= BTN_LONG_MS) {
            eventLong = true;
            longFired = true;
            clickCount = 0; // Cancel pending short
        }
        if (!powerFired && held >= BTN_POWEROFF_MS) {
            eventPowerOff = true;
            powerFired = true;
        }
    } else {
        longFired = false;
        powerFired = false;
        
        // Timeout for short click
        if (clickCount > 0 && (now - lastReleaseMs) >= 300) {
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
    if (!isrPressed) return 0;
    return millis() - isrPressStart;
}
