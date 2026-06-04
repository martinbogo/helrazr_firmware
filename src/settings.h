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

// Settings screen: a cursor list of configurable items.
//   short press  -> move cursor between settings
//   double press -> change the highlighted setting's value
//   long press   -> back to the main menu
void settings_enter();
void settings_update();
void settings_short_press();
void settings_double_press();
