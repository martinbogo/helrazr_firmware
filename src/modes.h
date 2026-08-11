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
    const char* name;      // short display name (fits the UI columns)
    const char* meshName;  // canonical Meshtastic name, used by the slot hash
    float       bwKHz;
    uint8_t     sf;
    uint8_t     cr;
};

// Meshtastic modem presets. These are region-independent (name / bandwidth /
// spreading factor / coding rate); the actual centre frequency depends on the
// selected region and is computed at runtime via mesh_channel_freq(), so the
// preset always lands on the frequency a real Meshtastic node would use in the
// current region. `meshName` is the canonical channel name the slot hash uses,
// which may differ from the short `name` shown on screen.
static const MeshChannel MESH_CHANNELS[] = {
    { "LongFast",  "LongFast",   250.0f, 11, 8 },
    { "LongMod",   "LongMod",    125.0f, 11, 8 },
    { "LongSlow",  "LongSlow",   125.0f, 12, 8 },
    { "MedFast",   "MediumFast", 250.0f,  9, 8 },
    { "MedSlow",   "MediumSlow", 250.0f, 10, 8 },
    { "ShortFast", "ShortFast",  250.0f,  7, 5 },
    { "ShortSlow", "ShortSlow",  250.0f,  8, 5 },
};
static const int MESH_CHANNEL_COUNT = 7;

// Centre frequency (MHz) of preset `idx` in the currently selected region.
float mesh_channel_freq(int idx);
