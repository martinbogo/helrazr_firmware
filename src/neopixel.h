#pragma once
#include <Arduino.h>

void neopixel_init();
void neopixel_update(bool gpsFix, bool loraRx);
void neopixel_set_color(uint8_t r, uint8_t g, uint8_t b); // override both pixels
void neopixel_off();
