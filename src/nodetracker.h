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

static const int MAX_NODES = 20;

struct NodeEntry {
    uint32_t nodeId;
    float    lastRSSI;
    float    bestRSSI;
    uint16_t packets;
    uint32_t firstSeenMs;
    uint32_t lastSeenMs;
};

// Called by decoder/monitor whenever a packet is parsed
void nodetracker_update(uint32_t nodeId, float rssi, float snr);

int              nodetracker_count();
const NodeEntry* nodetracker_get(int idx);  // sorted by lastSeenMs descending
void             nodetracker_clear();

// Mode lifecycle
void nodetracker_enter();
void nodetracker_mode_update();  // button-scrollable display
