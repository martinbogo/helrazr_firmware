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

static const MeshChannel MESH_CHANNELS[] = {
    { "LongFast",  906.875f, 250.0f, 11, 8 },
    { "LongMod",   906.875f, 125.0f, 11, 8 },
    { "LongSlow",  906.875f, 125.0f, 12, 8 },
    { "MedFast",   906.875f, 250.0f,  9, 8 },
    { "MedSlow",   906.875f, 250.0f, 10, 8 },
    { "ShortFast", 906.875f, 250.0f,  7, 5 },
    { "ShortSlow", 906.875f, 250.0f,  8, 5 },
};
static const int MESH_CHANNEL_COUNT = 7;
