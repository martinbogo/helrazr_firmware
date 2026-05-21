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

// Call at the very top of setup() -- may not return if device was woken from
// standby and the button is released before the 5-second hold threshold.
void power_init();

// Call every loop() iteration. Shows the power-off countdown overlay when the
// button is held >=5s and triggers standby at 10s.
void power_update();

// Graceful shutdown: turns off peripherals and enters deep sleep.
// Wakeup is via button hold (>=5s on the next boot to stay awake).
void power_standby();
