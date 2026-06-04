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

// Named colour themes. Each theme is a full palette swapped into the runtime
// DISPLAY_* tokens (TFT); on the monochrome OLED a theme may dim the panel.
// Add a theme by appending to the THEMES table in theme.cpp.
void        theme_init();             // load persisted choice and apply
int         theme_count();
const char* theme_name(int i);
int         theme_current();
void        theme_set(int i);         // clamp, apply, persist
void        theme_next();             // cycle to the next theme

// True for bright daytime themes (Hiking/Aviation/Mono): secondary UI such as
// the compass rose should render in bright white. False for Night/Amber, which
// keep their themed (dimmer/coloured) look.
bool        theme_bright_ui();
