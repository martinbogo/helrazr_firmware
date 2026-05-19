#include "neopixel.h"
#include "pins.h"

#ifdef PIN_NEOPIXEL

#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel pixels(NEOPIXEL_NUM, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

static uint32_t lastRxMs    = 0;
static uint32_t lastPulseMs = 0;
static bool     pulseOn     = false;

static const uint32_t RX_FLASH_MS  = 200;   // blue flash duration on RX
static const uint32_t PULSE_PERIOD = 1000;  // no-fix red pulse period

void neopixel_init() {
    pixels.begin();
    pixels.setBrightness(40);  // 0-255, keep dim to save power
    pixels.clear();
    pixels.show();
}

void neopixel_update(bool gpsFix, bool loraRx) {
    uint32_t now = millis();

    if (loraRx) lastRxMs = now;

    uint32_t col0, col1;

    // Pixel 0: GPS status
    if (gpsFix) {
        col0 = pixels.Color(0, 60, 0);   // green
    } else {
        // Pulse red slowly
        bool pulse = ((now / (PULSE_PERIOD / 2)) & 1);
        col0 = pulse ? pixels.Color(60, 0, 0) : pixels.Color(0, 0, 0);
    }

    // Pixel 1: LoRa RX flash (blue), else mirrors GPS status dimly
    if (now - lastRxMs < RX_FLASH_MS) {
        col1 = pixels.Color(0, 0, 80);   // blue flash
    } else {
        col1 = col0;
    }

    pixels.setPixelColor(0, col0);
    pixels.setPixelColor(1, col1);
    pixels.show();
}

void neopixel_set_color(uint8_t r, uint8_t g, uint8_t b) {
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.setPixelColor(1, pixels.Color(r, g, b));
    pixels.show();
}

void neopixel_off() {
    pixels.clear();
    pixels.show();
}

#else

void neopixel_init() {}
void neopixel_update(bool gpsFix, bool loraRx) {}
void neopixel_set_color(uint8_t r, uint8_t g, uint8_t b) {}
void neopixel_off() {}

#endif

