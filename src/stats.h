#pragma once
#include <Arduino.h>

void stats_record_packet(uint32_t nodeId, float rssi);
void stats_enter();
void stats_update();
