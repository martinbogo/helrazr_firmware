#include "variant.h"
#include "Arduino.h"
#include "nrf.h"
#include "wiring_constants.h"
#include "wiring_digital.h"

// 1:1 pin mapping (Arduino pin N = nRF52840 GPIO N)
// except pins 0,1 which are hardwired for 32MHz crystal
const uint32_t g_ADigitalPinMap[] = {
    // P0
    0xff, 0xff,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
      16,   17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    // P1
      32,   33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47
};

void initVariant() {
    pinMode(PIN_LED1, OUTPUT);
    digitalWrite(PIN_LED1, HIGH);  // LED off (active low)
}
