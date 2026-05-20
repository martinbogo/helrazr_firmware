#pragma once
#include <Arduino.h>

static const uint32_t BTN_LONG_MS     =   600;   // long press threshold
static const uint32_t BTN_WARN_MS     =  5000;   // show power-off warning
static const uint32_t BTN_POWEROFF_MS = 10000;   // hard power off
static const uint32_t BTN_WAKE_MS     =  5000;   // hold to wake from standby

void button_init();
void button_update();

bool     button_short_pressed();    // true once per short press (<600ms)
bool     button_double_pressed();   // true once per double press
bool     button_long_pressed();     // true once per long press (600ms, fires while held)
bool     button_poweroff_pressed(); // true once at 10s hold (fires while held)
uint32_t button_held_ms();          // current continuous hold duration (0 if not held)
