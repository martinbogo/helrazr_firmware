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

// Handheld-GPS view: a set of pages cycled with the button.
//   short press  -> next page
//   double press -> context action (cycle target / mark waypoint)
//   long press   -> exit to menu (handled by the main loop)
void gpsview_enter();
void gpsview_update();
void gpsview_short_press();
void gpsview_double_press();

// Long-press handler. Returns true if consumed internally (closed the context
// menu, or stepped back to the first page); false means "exit to main menu".
bool gpsview_long_press();
