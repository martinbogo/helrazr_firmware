#pragma once
#include <Arduino.h>

void gps_init();
void gps_update();

float gps_latitude();
float gps_longitude();
float gps_altitude();
float gps_speed_kmh();
int   gps_satellites();
bool  gps_has_fix();
uint32_t gps_chars_processed();
