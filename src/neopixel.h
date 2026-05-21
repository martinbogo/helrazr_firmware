/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#pragma once
#include <Arduino.h>

void neopixel_init();
void neopixel_update(bool gpsFix, bool loraRx);
void neopixel_set_color(uint8_t r, uint8_t g, uint8_t b); // override both pixels
void neopixel_off();
