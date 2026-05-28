/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "stats.h"
#include "display.h"




// Rolling 60-bucket ring buffer, one bucket = 10 seconds
static const int BUCKETS    = 60;
static const int BUCKET_SEC = 10;

static uint8_t  buckets[BUCKETS];   // packet count per bucket
static int      bucketHead = 0;
static uint32_t bucketStartMs = 0;
static uint32_t totalPackets = 0;

// Top-5 nodes by packet count
static const int TOP_NODES = 5;
struct NodeStat { uint32_t id; uint16_t count; float avgRSSI; float rssiSum; };
static NodeStat topNodes[TOP_NODES * 2];  // oversize, sorted by count
static int      nodeStatCount = 0;
static int      statsPage = 0; // 0 = Activity Graph, 1 = Top Nodes

static uint32_t lastRedraw = 0;

void stats_record_packet(uint32_t nodeId, float rssi) {
    totalPackets++;

    // Roll buckets if needed
    uint32_t now = millis();
    if (bucketStartMs == 0) bucketStartMs = now;
    uint32_t elapsed = (now - bucketStartMs) / 1000;
    while (elapsed >= BUCKET_SEC) {
        bucketHead = (bucketHead + 1) % BUCKETS;
        buckets[bucketHead] = 0;
        bucketStartMs += BUCKET_SEC * 1000;
        elapsed -= BUCKET_SEC;
    }
    buckets[bucketHead]++;

    // Update per-node stats
    for (int i = 0; i < nodeStatCount; i++) {
        if (topNodes[i].id == nodeId) {
            topNodes[i].count++;
            topNodes[i].rssiSum += rssi;
            topNodes[i].avgRSSI = topNodes[i].rssiSum / topNodes[i].count;
            return;
        }
    }
    if (nodeStatCount < TOP_NODES * 2) {
        topNodes[nodeStatCount++] = { nodeId, 1, rssi, rssi };
    }
}

static int packetsPerMinute() {
    // Sum last 6 buckets = 60 seconds
    int sum = 0;
    for (int i = 0; i < 6; i++) {
        sum += buckets[(bucketHead - i + BUCKETS) % BUCKETS];
    }
    return sum;
}

static void drawStats() {
    display_clear();

    char buf[40];

#if HAS_OLED
    display_draw_text_abs(20, 0, DISPLAY_CYAN, statsPage == 0 ? "Stats: Activity" : "Stats: Top Nodes");
    display_draw_hline(0, 10, 128, DISPLAY_GRAY);
#else
    display_draw_text_line(50, 15, DISPLAY_CYAN, statsPage == 0 ? "Stats: Activity" : "Stats: Top Nodes");
#endif

    if (statsPage == 0) {
        int ppm = packetsPerMinute();

#if HAS_OLED
        snprintf(buf, sizeof(buf), "Total: %-4lu   /m: %-3d", totalPackets, ppm);
        display_draw_text_abs(0, 12, DISPLAY_WHITE, buf);
        static const int GX = 0, GY = 25, GW = 128, GH = 25;
        display_fill_rect_abs(GX, GY, GW, GH, DISPLAY_BLACK);
#else
        snprintf(buf, sizeof(buf), "Total: %lu   Last min: %d", totalPackets, ppm);
        display_draw_text_line(0, 32, DISPLAY_WHITE, buf);
        static const int GX = 0, GY = 55, GW = 240, GH = 60;
        display_fill_rect_abs(GX, GY, GW, GH, DISPLAY_BLACK);
#endif
        display_draw_hline(GX, GY + GH, GW, DISPLAY_GRAY);

        uint8_t maxVal = 1;
        for (int i = 0; i < 30; i++) {
            uint8_t v = buckets[(bucketHead - i + BUCKETS) % BUCKETS];
            if (v > maxVal) maxVal = v;
        }
        for (int i = 0; i < 30; i++) {
            uint8_t v = buckets[(bucketHead - i + BUCKETS) % BUCKETS];
            int h = (int)((float)v / maxVal * GH);
            if (h > 0) {
                int bar_idx = 29 - i;
                int x0 = GX + (bar_idx       * GW) / 30;
                int x1 = GX + ((bar_idx + 1) * GW) / 30;
                int bw = x1 - x0 - 1;
                if (bw < 1) bw = 1;
                uint16_t col = (i < 6) ? DISPLAY_GREEN : (i < 18 ? DISPLAY_CYAN : DISPLAY_GRAY);
                display_fill_rect_abs(x0, GY + GH - h, bw, h, col);
            }
        }

#if HAS_OLED
        display_draw_text_small_abs(0, GY + GH + 2, DISPLAY_CYAN, "now");
        display_draw_text_small_abs(110, GY + GH + 2, DISPLAY_CYAN, "5m");
#else
        display_begin_line(GY + GH + 6, false);
        display_line_text(0, DISPLAY_CYAN, "now");
        display_line_text(200, DISPLAY_CYAN, "5min");
        display_end_line();
#endif
    } else {
        for (int i = 1; i < nodeStatCount; i++) {
            NodeStat k = topNodes[i]; int j = i - 1;
            while (j >= 0 && topNodes[j].count < k.count) { topNodes[j+1] = topNodes[j]; j--; }
            topNodes[j+1] = k;
        }

#if HAS_OLED
        display_draw_text_small_abs(0, 15, DISPLAY_CYAN, "Node ID   Pkts  RSSI");
        display_draw_hline(0, 24, 128, DISPLAY_GRAY);
        int shown = min(nodeStatCount, 4);
        for (int i = 0; i < shown; i++) {
            snprintf(buf, sizeof(buf), "%08lX  %4d  %4d",
                     topNodes[i].id, topNodes[i].count, (int)topNodes[i].avgRSSI);
            display_draw_text_small_abs(0, 28 + i * 9, i == 0 ? DISPLAY_WHITE : (uint16_t)DISPLAY_CYAN, buf);
        }
        if (nodeStatCount == 0) {
            display_draw_text_small_abs(20, 30, DISPLAY_CYAN, "No packets yet");
        }
#else
        display_draw_text_line(0, 32, DISPLAY_CYAN, "Node ID   Pkts  AvgRSSI");
        display_draw_hline(0, 48, 240, DISPLAY_GRAY);

        int shown = min(nodeStatCount, TOP_NODES);
        for (int i = 0; i < shown; i++) {
            snprintf(buf, sizeof(buf), "%08lX  %4d  %4d dBm",
                     topNodes[i].id, topNodes[i].count, (int)topNodes[i].avgRSSI);
            display_draw_text_line(0, 68 + i * 18, i == 0 ? DISPLAY_WHITE : (uint16_t)DISPLAY_CYAN, buf);
        }
        for (int i = shown; i < TOP_NODES; i++) {
            display_draw_text_line(0, 68 + i * 18, DISPLAY_BLACK, "");
        }
        if (nodeStatCount == 0) {
            display_draw_text_line(50, 68, DISPLAY_CYAN, "No packets yet");
        }
#endif
    }
    display_update_buffer();
}

void stats_short_press() {
    statsPage = (statsPage + 1) % 2;
    display_clear(true);
    drawStats();
}

void stats_enter() {
    lastRedraw = 0;
    drawStats();
}

void stats_update() {
    uint32_t now = millis();
    if (now - lastRedraw > 2000) {
        lastRedraw = now;
        drawStats();
    }
}
