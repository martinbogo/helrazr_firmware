/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "nodetracker.h"
#include "button.h"
#include "display.h"




static NodeEntry nodes[MAX_NODES];
static int       nodeCount  = 0;
static int       scrollTop  = 0;  // first visible row in display
#if HAS_OLED
static const int ROWS_SHOWN = 3;
#else
static const int ROWS_SHOWN = 5;
#endif

// --- Data management ---

void nodetracker_update(uint32_t nodeId, float rssi, float snr) {
    (void)snr;
    for (int i = 0; i < nodeCount; i++) {
        if (nodes[i].nodeId == nodeId) {
            nodes[i].lastRSSI   = rssi;
            if (rssi > nodes[i].bestRSSI) nodes[i].bestRSSI = rssi;
            nodes[i].packets++;
            nodes[i].lastSeenMs = millis();
            return;
        }
    }
    if (nodeCount < MAX_NODES) {
        nodes[nodeCount++] = {
            nodeId, rssi, rssi, 1, millis(), millis()
        };
    }
}

int nodetracker_count() { return nodeCount; }

const NodeEntry* nodetracker_get(int idx) {
    if (idx < 0 || idx >= nodeCount) return nullptr;
    return &nodes[idx];
}

void nodetracker_clear() { nodeCount = 0; scrollTop = 0; }

// --- Display ---

static void drawTable() {
    display_clear();

    char title[32];
#if HAS_OLED
    snprintf(title, sizeof(title), "Nodes (%d)", nodeCount);
    display_draw_text_abs(0, 0, DISPLAY_CYAN, title);
    display_draw_hline(0, 9, 128, DISPLAY_GRAY);
#else
    snprintf(title, sizeof(title), "Nodes (%d seen)", nodeCount);
    display_draw_text_line(0, 15, DISPLAY_CYAN, title);
    display_draw_hline(0, 20, 240, 0x2104);
#endif

    uint32_t now = millis();
    int shown = min(ROWS_SHOWN, nodeCount - scrollTop);
    for (int i = 0; i < shown; i++) {
        const NodeEntry& n = nodes[scrollTop + i];
#if HAS_OLED
        int y = 12 + i * 10;
#else
        int y = 38 + i * 19;
#endif

        char age[16];
        uint32_t secs = (now - n.lastSeenMs) / 1000;
        if (secs < 60)        snprintf(age, sizeof(age), "%lus", secs);
        else if (secs < 3600) snprintf(age, sizeof(age), "%lum", secs / 60);
        else                  snprintf(age, sizeof(age), "%luh", secs / 3600);

#if HAS_OLED
        char row[40];
        snprintf(row, sizeof(row), "%08lX %3d %4d %3s",
                 n.nodeId, n.packets, (int)n.lastRSSI, age);
        display_draw_text_small_abs(0, y, DISPLAY_WHITE, row);
#else
        char nodeId[12], rssi[8];
        snprintf(nodeId, sizeof(nodeId), "%08lX", n.nodeId);
        snprintf(rssi, sizeof(rssi), "%d", (int)n.lastRSSI);

        display_begin_line(y, false);
        display_line_text(0, DISPLAY_WHITE, nodeId);
        display_line_text(110, DISPLAY_WHITE, rssi);
        display_line_text(175, DISPLAY_CYAN, age);
        display_end_line();
#endif
    }

#if !HAS_OLED
    for (int i = shown; i < ROWS_SHOWN; i++) {
        display_begin_line(38 + i * 19, false);
        display_end_line();
    }
#endif

    if (nodeCount == 0) {
#if HAS_OLED
        display_draw_text_abs(10, 34, DISPLAY_CYAN, "No nodes yet");
#else
        display_draw_text_line(30, 80, DISPLAY_CYAN, "No nodes heard yet");
#endif
    }

    char foot[40];
    if (nodeCount > ROWS_SHOWN) {
#if HAS_OLED
        snprintf(foot, sizeof(foot), "%d-%d of %d  S:scroll",
                 scrollTop + 1, scrollTop + min(ROWS_SHOWN, nodeCount - scrollTop), nodeCount);
#else
        snprintf(foot, sizeof(foot), "%d-%d of %d  Short:scroll",
                 scrollTop + 1, scrollTop + min(ROWS_SHOWN, nodeCount - scrollTop), nodeCount);
#endif
    } else {
#if HAS_OLED
        snprintf(foot, sizeof(foot), "%d node%s  L:back",
                 nodeCount, nodeCount == 1 ? "" : "s");
#else
        snprintf(foot, sizeof(foot), "%d node%s  Long:back",
                 nodeCount, nodeCount == 1 ? "" : "s");
#endif
    }
#if HAS_OLED
    display_draw_text_small_abs(0, 55, DISPLAY_CYAN, foot);
#else
    display_draw_text_small_line(0, 133, DISPLAY_CYAN, foot);
#endif
}

void nodetracker_enter() {
    scrollTop = 0;
    drawTable();
}

void nodetracker_mode_update() {
    if (button_short_pressed()) {
        if (nodeCount > ROWS_SHOWN) {
            scrollTop += ROWS_SHOWN;
            if (scrollTop >= nodeCount) scrollTop = 0;
            drawTable();
        }
    }
    // Refresh every 5 seconds to update ages
    static uint32_t lastRefresh = 0;
    uint32_t now = millis();
    if (now - lastRefresh > 5000) {
        lastRefresh = now;
        drawTable();
    }
}
