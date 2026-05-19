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
#if HAS_OLED
    display_draw_text_abs(20, 0, DISPLAY_CYAN, "Packet Stats");
    display_draw_hline(0, 10, 128, DISPLAY_GRAY);
#else
    display_draw_text_abs(60, 15, DISPLAY_CYAN, "Packet Stats");
    display_draw_hline(0, 20, 240, DISPLAY_GRAY);
#endif

    char buf[40];
    int ppm = packetsPerMinute();

#if HAS_OLED
    // OLED layout (128x64). All rows on 8px grid, no overlaps:
    //  y= 0: title (already drawn)
    //  y= 9: hline
    //  y=11: compact total + per-min  (max 21 chars * 6px = 126px)
    //  y=20..37: bar graph (GH=17)
    //  y=38: hline (graph bottom axis)
    //  y=40: x-axis labels "now" / "5m"   (occupies y=40..47)
    //  y=49: hline  (2px clear gap after label row)
    //  y=51: top node data              (occupies y=51..58)
    snprintf(buf, sizeof(buf), "Total:%-4lu /m:%-3d", totalPackets, ppm);
    display_draw_text_small_abs(0, 11, DISPLAY_WHITE, buf);

    static const int GX = 0, GY = 20, GW = 128, GH = 17;
#else
    snprintf(buf, sizeof(buf), "Total: %lu   Last min: %d", totalPackets, ppm);
    display_draw_text_small_abs(0, 32, DISPLAY_WHITE, buf);

    static const int GX = 0, GY = 36, GW = 240, GH = 40;
#endif
    display_fill_rect_abs(GX, GY, GW, GH, DISPLAY_BLACK);
    display_draw_hline(GX, GY + GH, GW, DISPLAY_GRAY);

    // Find max bucket for scaling
    uint8_t maxVal = 1;
    for (int i = 0; i < 30; i++) {
        uint8_t v = buckets[(bucketHead - i + BUCKETS) % BUCKETS];
        if (v > maxVal) maxVal = v;
    }
    // Proportional bar placement fills the full graph width exactly
    for (int i = 0; i < 30; i++) {
        uint8_t v = buckets[(bucketHead - i + BUCKETS) % BUCKETS];
        int h = (int)((float)v / maxVal * GH);
        if (h > 0) {
            int bar_idx = 29 - i;  // bar_idx=0 at left (oldest), bar_idx=29 at right (newest)
            int x0 = GX + (bar_idx       * GW) / 30;
            int x1 = GX + ((bar_idx + 1) * GW) / 30;
            int bw = x1 - x0 - 1;
            if (bw < 1) bw = 1;
            uint16_t col = (i < 6) ? DISPLAY_GREEN : (i < 18 ? DISPLAY_CYAN : DISPLAY_GRAY);
            display_fill_rect_abs(x0, GY + GH - h, bw, h, col);
        }
    }

#if HAS_OLED
    // x-axis labels at y=40 (row occupies y=40..47, safely clear of hline at y=38)
    display_draw_text_small_abs(0,   GY + GH + 2, DISPLAY_CYAN, "now");
    display_draw_text_small_abs(110, GY + GH + 2, DISPLAY_CYAN, "5m");
    // Separator after label row (y=49, 2px after text bottom at y=47)
    display_draw_hline(0, GY + GH + 11, GW, DISPLAY_GRAY);
#else
    display_draw_text_small_abs(0,   GY + GH + 6, DISPLAY_CYAN, "now");
    display_draw_text_small_abs(190, GY + GH + 6, DISPLAY_CYAN, "5min");
    display_draw_hline(0, 92, 240, DISPLAY_GRAY);
    display_draw_text_small_abs(0, 102, DISPLAY_CYAN, "Node ID   Pkts  AvgRSSI");
#endif

    // Sort topNodes by count
    for (int i = 1; i < nodeStatCount; i++) {
        NodeStat k = topNodes[i]; int j = i - 1;
        while (j >= 0 && topNodes[j].count < k.count) { topNodes[j+1] = topNodes[j]; j--; }
        topNodes[j+1] = k;
    }
#if HAS_OLED
    // Top node at y=51: "%08lX %3d %3ddBm" = 18 chars * 6px = 108px, fits
    if (nodeStatCount > 0) {
        snprintf(buf, sizeof(buf), "%08lX %3d %3ddBm",
                 topNodes[0].id, topNodes[0].count, (int)topNodes[0].avgRSSI);
        display_draw_text_small_abs(0, GY + GH + 13, DISPLAY_WHITE, buf);
    } else {
        display_draw_text_small_abs(8, GY + GH + 13, DISPLAY_CYAN, "No packets yet");
    }
#else
    int shown = min(nodeStatCount, TOP_NODES);
    for (int i = 0; i < shown; i++) {
        snprintf(buf, sizeof(buf), "%08lX  %4d  %4d dBm",
                 topNodes[i].id, topNodes[i].count, (int)topNodes[i].avgRSSI);
        display_draw_text_small_abs(0, 112 + i * 10, i == 0 ? DISPLAY_WHITE : (uint16_t)DISPLAY_CYAN, buf);
    }
    if (nodeStatCount == 0) {
        display_draw_text_small_abs(30, 112, DISPLAY_CYAN, "No packets yet");
    }
#endif
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
