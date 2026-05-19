#include "decoder.h"
#include "lora.h"
#include "modes.h"
#include "display.h"
#include "nodetracker.h"




struct MeshHeader {
    uint32_t destNode, srcNode, packetId;
    uint8_t  flags, channelHash;
};

struct LogEntry {
    uint32_t srcNode, destNode;
    uint8_t  hopLimit;
    float    rssi, snr;
    bool     hasText;
    char     text[48];
    uint32_t timeMs;
};

static const int LOG_SIZE = 3;
static LogEntry  log_buf[LOG_SIZE];
static int       logHead = 0;
static int       totalPkts = 0;

static void addLog(const LogEntry& e) {
    log_buf[logHead % LOG_SIZE] = e;
    logHead++; totalPkts++;
}

static bool tryDecodeText(const uint8_t* payload, int len, char* out, int outlen) {
    int i = 0;
    while (i < len - 1) {
        uint8_t tag = payload[i++];
        uint8_t field = tag >> 3, wire = tag & 0x07;
        if (wire == 0) {
            while (i < len && (payload[i++] & 0x80));
        } else if (wire == 2) {
            if (i >= len) break;
            uint8_t slen = payload[i++];
            if (slen > len - i) break;
            if (field == 2 && slen > 0 && slen < (uint8_t)outlen) {
                memcpy(out, &payload[i], slen); out[slen] = '\0';
                bool ok = true;
                for (int k = 0; k < slen; k++) if (out[k] < 0x20 && out[k] != '\n') { ok = false; break; }
                if (ok) return true;
                out[0] = '\0';
            }
            i += slen;
        } else break;
    }
    return false;
}

static void drawLog() {
#if HAS_OLED
    display_fill_rect_abs(0, 10, 128, 54, DISPLAY_BLACK);
#else
    display_fill_rect_abs(0, 18, 240, 117, DISPLAY_BLACK);
#endif

    if (totalPkts == 0) {
#if HAS_OLED
        display_draw_text_abs(5, 30, DISPLAY_CYAN, "Waiting for packets...");
#else
        display_draw_text_abs(20, 75, DISPLAY_CYAN, "Waiting for packets...");
#endif
        return;
    }

    int recent = (logHead - 1 + LOG_SIZE) % LOG_SIZE;
    LogEntry& e = log_buf[recent];
    char line[40];

    // Most recent packet
#if HAS_OLED
    snprintf(line, sizeof(line), "Frm %08lX", e.srcNode);
    display_draw_text_small_abs(0, 12, DISPLAY_GREEN, line);

    snprintf(line, sizeof(line), "R:%d S:%d", (int)e.rssi, (int)e.snr);
    display_draw_text_small_abs(0, 21, DISPLAY_YELLOW, line);

    if (e.hasText) {
        display_draw_text_small_abs(0, 30, DISPLAY_CYAN, "MSG:");
        char wrap[24]; strncpy(wrap, e.text, 23); wrap[23] = '\0';
        display_draw_text_small_abs(24, 30, DISPLAY_WHITE, wrap);
    }

    display_draw_hline(0, 39, 128, DISPLAY_GRAY);
    int shown = totalPkts < 2 ? totalPkts : 2;
    for (int i = 0; i < shown; i++) {
        int idx = (logHead - 1 - i + LOG_SIZE * 2) % LOG_SIZE;
        LogEntry& le = log_buf[idx];
        uint32_t age = (millis() - le.timeMs) / 1000;
        snprintf(line, sizeof(line), "%08lX %lus", le.srcNode, age);
        display_draw_text_small_abs(0, 40 + i * 8, i == 0 ? DISPLAY_WHITE : (uint16_t)DISPLAY_CYAN, line);
    }

    char foot[32];
    snprintf(foot, sizeof(foot), "LF Pkts: %d", totalPkts);
    display_draw_text_small_abs(70, 56, DISPLAY_CYAN, foot);
#else
    snprintf(line, sizeof(line), "From %08lX -> %08lX", e.srcNode, e.destNode);
    display_draw_text_abs(0, 33, DISPLAY_GREEN, line);

    snprintf(line, sizeof(line), "Hop:%d  RSSI:%d  SNR:%d",
             e.hopLimit, (int)e.rssi, (int)e.snr);
    display_draw_text_abs(0, 51, DISPLAY_YELLOW, line);

    if (e.hasText) {
        display_draw_text_small_abs(0, 63, DISPLAY_CYAN, "MSG:");
        char wrap[30]; strncpy(wrap, e.text, 29); wrap[29] = '\0';
        display_draw_text_small_abs(30, 63, DISPLAY_WHITE, wrap);
        if (strlen(e.text) > 29) {
            strncpy(wrap, e.text + 29, 29); wrap[29] = '\0';
            display_draw_text_small_abs(0, 72, DISPLAY_WHITE, wrap);
        }
    }

    // Log entries
    display_draw_hline(0, 80, 240, DISPLAY_GRAY);
    int shown = totalPkts < 3 ? totalPkts : 3;
    for (int i = 0; i < shown; i++) {
        int idx = (logHead - 1 - i + LOG_SIZE * 2) % LOG_SIZE;
        LogEntry& le = log_buf[idx];
        uint32_t age = (millis() - le.timeMs) / 1000;
        snprintf(line, sizeof(line), "%08lX->%08lX %lus", le.srcNode, le.destNode, age);
        display_draw_text_small_abs(0, 89 + i * 12, i == 0 ? DISPLAY_WHITE : (uint16_t)DISPLAY_CYAN, line);
    }

    display_fill_rect_abs(0, 122, 240, 13, DISPLAY_BLACK);
    char foot[32];
    snprintf(foot, sizeof(foot), "LongFast  Pkts: %d", totalPkts);
    display_draw_text_small_abs(0, 133, DISPLAY_CYAN, foot);
#endif
}

void decoder_enter() {
    logHead = 0; totalPkts = 0;
    memset(log_buf, 0, sizeof(log_buf));
    lora_apply_channel(0);
    lora_start_listen();
    display_clear();
#if HAS_OLED
    display_draw_text_abs(10, 0, DISPLAY_CYAN, "Meshtastic Decoder");
    display_draw_hline(0, 10, 128, DISPLAY_GRAY);
#else
    display_draw_text_abs(30, 15, DISPLAY_CYAN, "Meshtastic Decoder");
    display_draw_hline(0, 20, 240, DISPLAY_GRAY);
#endif
    drawLog();
}

void decoder_update() {
    if (!lora_poll_packet()) return;

    uint8_t buf[256]; int len = 0;
    lora_get_last_packet(buf, &len);
    if (len < 16) return;

    MeshHeader hdr;
    memcpy(&hdr.destNode,  buf + 0, 4);
    memcpy(&hdr.srcNode,   buf + 4, 4);
    memcpy(&hdr.packetId,  buf + 8, 4);
    hdr.flags = buf[12]; hdr.channelHash = buf[13];
    uint8_t hopLimit = (hdr.flags >> 1) & 0x07;

    LogEntry entry = {};
    entry.srcNode  = hdr.srcNode;
    entry.destNode = hdr.destNode;
    entry.hopLimit = hopLimit;
    entry.rssi     = lora_last_rssi();
    entry.snr      = lora_last_snr();
    entry.timeMs   = millis();
    if (len > 16) entry.hasText = tryDecodeText(buf + 16, len - 16, entry.text, sizeof(entry.text));

    addLog(entry);
    nodetracker_update(hdr.srcNode, entry.rssi, entry.snr);
    drawLog();

    Serial.println("--- Meshtastic Packet ---");
    Serial.print("  From:    0x"); Serial.println(hdr.srcNode, HEX);
    Serial.print("  To:      0x"); Serial.println(hdr.destNode, HEX);
    Serial.print("  HopLim:  ");   Serial.println(hopLimit);
    Serial.print("  RSSI:    ");   Serial.print(entry.rssi);   Serial.println(" dBm");
    Serial.print("  SNR:     ");   Serial.print(entry.snr);    Serial.println(" dB");
    if (entry.hasText) { Serial.print("  Text: \""); Serial.print(entry.text); Serial.println("\""); }
    Serial.print("  Raw["); Serial.print(len); Serial.print("]: ");
    for (int i = 0; i < len && i < 32; i++) {
        if (buf[i] < 0x10) Serial.print('0');
        Serial.print(buf[i], HEX); Serial.print(' ');
    }
    if (len > 32) Serial.print("...");
    Serial.println();
}
