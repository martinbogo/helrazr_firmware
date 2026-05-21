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
    // OLED: 128px wide, 21 chars max per row at 6px/char
    // "Nodes (20)" = 10 chars = 60px -- short, leaves room
    snprintf(title, sizeof(title), "Nodes (%d)", nodeCount);
    display_draw_text_abs(0, 0, DISPLAY_CYAN, title);
    display_draw_hline(0, 9, 128, DISPLAY_GRAY);
    // No column header -- use the space for data rows instead
    // Row layout (3 rows): y=12, 22, 32  footer: y=55
    // Format "%08lX %3d %4d %3s" = 21 chars max = 126px ✓
#else
    snprintf(title, sizeof(title), "Node Tracker  (%d seen)", nodeCount);
    display_draw_text_abs(0, 15, DISPLAY_CYAN, title);
    display_draw_hline(0, 20, 240, DISPLAY_GRAY);
    display_draw_text_small_abs(0,   30, DISPLAY_CYAN, "Node ID   Pkts  RSSI  Age");
    display_draw_hline(0, 33, 240, 0x2104);
#endif

    uint32_t now = millis();
    int shown = min(ROWS_SHOWN, nodeCount - scrollTop);
    for (int i = 0; i < shown; i++) {
        const NodeEntry& n = nodes[scrollTop + i];
#if HAS_OLED
        int y = 12 + i * 10;   // y=12, 22, 32 -- last row bottom at y=39, footer at y=55
#else
        int y = 44 + i * 16;
#endif

        char age[8];
        uint32_t secs = (now - n.lastSeenMs) / 1000;
        if (secs < 60)        snprintf(age, sizeof(age), "%3lus", secs);
        else if (secs < 3600) snprintf(age, sizeof(age), "%3lum", secs / 60);
        else                  snprintf(age, sizeof(age), "%3luh", secs / 3600);

        char row[40];
#if HAS_OLED
        // 8+1+3+1+4+1+3 = 21 chars = 126px ✓
        snprintf(row, sizeof(row), "%08lX %3d %4d %3s",
                 n.nodeId, n.packets, (int)n.lastRSSI, age);
#else
        snprintf(row, sizeof(row), "%08lX  %4d  %4d  %s",
                 n.nodeId, n.packets, (int)n.lastRSSI, age);
#endif
        display_draw_text_small_abs(0, y, DISPLAY_WHITE, row);
    }

    if (nodeCount == 0) {
#if HAS_OLED
        display_draw_text_abs(10, 34, DISPLAY_CYAN, "No nodes yet");
#else
        display_draw_text_abs(30, 80, DISPLAY_CYAN, "No nodes heard yet");
#endif
    }

    // Footer
    char foot[40];
    if (nodeCount > ROWS_SHOWN) {
#if HAS_OLED
        // "1-3 of 12  S:scroll" = 19 chars = 114px ✓
        snprintf(foot, sizeof(foot), "%d-%d of %d  S:scroll",
                 scrollTop + 1, scrollTop + min(ROWS_SHOWN, nodeCount - scrollTop), nodeCount);
#else
        snprintf(foot, sizeof(foot), "%d-%d of %d  Short:scroll",
                 scrollTop + 1, scrollTop + min(ROWS_SHOWN, nodeCount - scrollTop), nodeCount);
#endif
    } else {
#if HAS_OLED
        // "3 nodes  L:back" = 15 chars = 90px ✓
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
    display_draw_text_small_abs(0, 133, DISPLAY_CYAN, foot);
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
