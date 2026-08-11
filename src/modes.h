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
#include "region.h"

enum AppMode {
    MODE_MENU = 0,
    MODE_STATUS,
    MODE_SPECTRUM,
    MODE_WATERFALL,
    MODE_NOISE,
    MODE_SCANNER,
    MODE_MONITOR,
    MODE_DUTY,
    MODE_FREQOFFSET,
    MODE_DECODER,
    MODE_NODES,
    MODE_STATS,
    MODE_AUTOTRACK,
    MODE_STANDBY,
    MODE_OTA,
    MODE_GPS,
    MODE_SETTINGS,
    MODE_COUNT
};

extern AppMode currentMode;

const char* mode_name(AppMode m);

struct MeshChannel {
    const char* name;
    float       freqMHz;
    float       bwKHz;
    uint8_t     sf;
    uint8_t     cr;
};

// Meshtastic modem presets. The centre frequency of each preset is derived from
// the selected region (see region.h) using Meshtastic's frequency-slot hash, so
// changing LORA_REGION re-targets every preset to the frequency a real
// Meshtastic node would use in that region. The first argument to
// MESH_CHANNEL_FREQ is the canonical Meshtastic channel name used by the slot
// hash (which may differ from the short display name in the .name field).
static const MeshChannel MESH_CHANNELS[] = {
    { "LongFast",  MESH_CHANNEL_FREQ("LongFast",   250), 250.0f, 11, 8 },
    { "LongMod",   MESH_CHANNEL_FREQ("LongMod",    125), 125.0f, 11, 8 },
    { "LongSlow",  MESH_CHANNEL_FREQ("LongSlow",   125), 125.0f, 12, 8 },
    { "MedFast",   MESH_CHANNEL_FREQ("MediumFast", 250), 250.0f,  9, 8 },
    { "MedSlow",   MESH_CHANNEL_FREQ("MediumSlow", 250), 250.0f, 10, 8 },
    { "ShortFast", MESH_CHANNEL_FREQ("ShortFast",  250), 250.0f,  7, 5 },
    { "ShortSlow", MESH_CHANNEL_FREQ("ShortSlow",  250), 250.0f,  8, 5 },
};
static const int MESH_CHANNEL_COUNT = 7;
