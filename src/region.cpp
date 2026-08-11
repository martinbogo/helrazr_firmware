/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "region.h"
#include "modes.h"
#include "lora.h"

#if defined(ESP32)
#include <Preferences.h>
static Preferences regionPrefs;
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;
static const char *REGION_PATH = "/region.dat";
#endif

// One row per regulatory region. Band edges (kHz, integer to keep the slot math
// exact) are taken verbatim from the Meshtastic firmware region table. `code` is
// the stable RG_* identifier persisted to flash. `selectable` is false for bands
// this board's radio cannot reach (2.4 GHz on a sub-GHz SX1262).
struct RegionDef {
    uint8_t     code;
    const char *name;
    long        startKHz;
    long        endKHz;
    bool        selectable;
};

static const RegionDef REGIONS[] = {
    { RG_US,      "US",      902000L,  928000L,  true  },
    { RG_EU_868,  "EU_868",  869400L,  869650L,  true  },
    { RG_EU_433,  "EU_433",  433000L,  434000L,  true  },
    { RG_CN,      "CN",      470000L,  510000L,  true  },
    { RG_JP,      "JP",      920500L,  923500L,  true  },
    { RG_ANZ,     "ANZ",     915000L,  928000L,  true  },
    { RG_ANZ_433, "ANZ_433", 433050L,  434790L,  true  },
    { RG_RU,      "RU",      868700L,  869200L,  true  },
    { RG_KR,      "KR",      920000L,  923000L,  true  },
    { RG_TW,      "TW",      920000L,  925000L,  true  },
    { RG_IN,      "IN",      865000L,  867000L,  true  },
    { RG_NZ_865,  "NZ_865",  864000L,  868000L,  true  },
    { RG_TH,      "TH",      920000L,  925000L,  true  },
    { RG_UA_433,  "UA_433",  433000L,  434700L,  true  },
    { RG_UA_868,  "UA_868",  868000L,  868600L,  true  },
    { RG_MY_433,  "MY_433",  433000L,  435000L,  true  },
    { RG_MY_919,  "MY_919",  919000L,  924000L,  true  },
    { RG_SG_923,  "SG_923",  917000L,  925000L,  true  },
    { RG_PH_433,  "PH_433",  433000L,  434700L,  true  },
    { RG_PH_868,  "PH_868",  868000L,  869400L,  true  },
    { RG_PH_915,  "PH_915",  915000L,  918000L,  true  },
    { RG_BR_902,  "BR_902",  902000L,  907500L,  true  },
    { RG_NP_865,  "NP_865",  865000L,  868000L,  true  },
    { RG_LORA_24, "LORA_24", 2400000L, 2483500L, false },  // needs SX128x radio
};
static const int REGION_N = sizeof(REGIONS) / sizeof(REGIONS[0]);

static int s_region = 0;  // index into REGIONS[]

// Table index for a stable RG_* code (falls back to index 0 / US).
static int index_for_code(int code) {
    for (int i = 0; i < REGION_N; i++)
        if (REGIONS[i].code == code) return i;
    return 0;
}

// --- Meshtastic frequency-slot algorithm ------------------------------------

// djb2 string hash (Dan Bernstein), matching Meshtastic RadioInterface.cpp.
static uint32_t rf_djb2(const char *s) {
    uint32_t h = 5381u;
    while (*s) h = ((h << 5) + h) + (uint8_t)*s++;
    return h;
}

// Centre frequency (MHz) of a channel name for arbitrary band edges.
static float slot_freq(const char *meshName, long startKHz, long endKHz, long bwKHz) {
    long numChannels = (endKHz - startKHz) / bwKHz;
    if (numChannels < 1) numChannels = 1;
    long slot = (long)(rf_djb2(meshName) % (uint32_t)numChannels);
    // freq = freqStart + bw/2 + slot * bw
    return (float)(((double)startKHz * 1000.0
                    + (double)bwKHz * 500.0
                    + (double)slot * (double)bwKHz * 1000.0)
                   / 1000000.0);
}

// --- Persistence (same idiom as theme.cpp) ----------------------------------

static void load() {
    int code = LORA_REGION;  // compile-time default
#if defined(ESP32)
    regionPrefs.begin("region", true);
    code = regionPrefs.getInt("code", code);
    regionPrefs.end();
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
    InternalFS.begin();
    File f(InternalFS);
    if (f.open(REGION_PATH, FILE_O_READ)) {
        code = f.read();
        f.close();
    }
#endif
    s_region = index_for_code(code);
}

static void store() {
    int code = REGIONS[s_region].code;
#if defined(ESP32)
    regionPrefs.begin("region", false);
    regionPrefs.putInt("code", code);
    regionPrefs.end();
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
    InternalFS.remove(REGION_PATH);
    File f(InternalFS);
    if (f.open(REGION_PATH, FILE_O_WRITE)) {
        uint8_t b = (uint8_t)code;
        f.write(&b, 1);
        f.close();
    }
#endif
}

// --- Public API -------------------------------------------------------------

void region_init() { load(); }  // radio not up yet; lora_init() reads the region

int         region_count()          { return REGION_N; }
const char* region_name(int idx)    { return (idx >= 0 && idx < REGION_N) ? REGIONS[idx].name : "?"; }
int         region_current()        { return s_region; }
int         region_current_code()   { return REGIONS[s_region].code; }
bool        region_selectable(int idx) { return (idx >= 0 && idx < REGION_N) && REGIONS[idx].selectable; }

float region_freq_start_mhz() { return (float)REGIONS[s_region].startKHz / 1000.0f; }
float region_freq_end_mhz()   { return (float)REGIONS[s_region].endKHz   / 1000.0f; }
float region_span_mhz()       { return region_freq_end_mhz() - region_freq_start_mhz(); }

float region_channel_freq(const char *meshName, int bwKHz) {
    return slot_freq(meshName, REGIONS[s_region].startKHz, REGIONS[s_region].endKHz, bwKHz);
}

// Frequency of a preset from the MESH_CHANNELS table (see modes.h).
float mesh_channel_freq(int idx) {
    if (idx < 0 || idx >= MESH_CHANNEL_COUNT) idx = 0;
    const MeshChannel &ch = MESH_CHANNELS[idx];
    return region_channel_freq(ch.meshName, (int)ch.bwKHz);
}

void region_set(int idx) {
    if (idx < 0) idx = 0;
    if (idx >= REGION_N) idx = REGION_N - 1;
    s_region = idx;
    store();
    // Retune the radio into the new band now, so the change takes effect
    // immediately. Screens that sweep the band re-read the edges on entry.
    lora_apply_region();
}

void region_next() {
    // Advance to the next *selectable* region (skips 2.4 GHz on sub-GHz radios).
    int idx = s_region;
    for (int i = 0; i < REGION_N; i++) {
        idx = (idx + 1) % REGION_N;
        if (REGIONS[idx].selectable) break;
    }
    region_set(idx);
}
