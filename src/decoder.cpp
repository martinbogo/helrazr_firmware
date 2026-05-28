/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "decoder.h"
#include "lora.h"
#include "modes.h"
#include "display.h"
#include "nodetracker.h"
#include <Crypto.h>
#include <AES.h>
#include <CTR.h>




struct MeshHeader {
    uint32_t destNode, srcNode, packetId;
    uint8_t  flags, channelHash;
};

struct LogEntry {
    uint32_t srcNode, destNode;
    uint8_t  hopLimit;
    float    rssi, snr;
    uint32_t portNum;
    bool     isCleartext;
    bool     hasText;
    char     text[240];
    uint32_t timeMs;
};


static const uint8_t DEFAULT_PSK[16] = {
    0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59, 
    0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01
};

static const int LOG_SIZE = 12;
static LogEntry  log_buf[LOG_SIZE];
static int       logHead = 0;
static int       totalPkts = 0;
static int       viewOffset = 0;
static bool      detailMode = false;
static int       payloadPage = 0;

static void drawLog();

static const char* getPortName(uint32_t portnum) {
    switch (portnum) {
        case 0: return "UNKNOWN";
        case 1: return "TEXT";
        case 2: return "REMOTE_HW";
        case 3: return "POSITION";
        case 4: return "NODEINFO";
        case 5: return "ROUTING";
        case 6: return "ADMIN";
        case 7: return "TEXT_CMPRS";
        case 8: return "WAYPOINT";
        case 9: return "AUDIO";
        case 64: return "TELEMETRY";
        case 65: return "ZPSK";
        case 66: return "SIMULATOR";
        case 67: return "STORE_FWD";
        case 68: return "RANGE_TEST";
        case 69: return "TRACEROUTE";
        case 70: return "NEIGHBOR";
        case 71: return "PAXCOUNTER";
        default: return "APP_DATA";
    }
}

static void addLog(const LogEntry& e) {
    log_buf[logHead % LOG_SIZE] = e;
    logHead++; totalPkts++;
    
    // Shift our viewed packet if we are looking back in time
    if (viewOffset > 0) {
        viewOffset++;
        if (viewOffset >= LOG_SIZE) viewOffset = 0; // If pushed out of buffer, jump to live mode
    }
}

static bool parseDataProto(const uint8_t* payload, int len, LogEntry& entry) {
    int i = 0;
    entry.portNum = 0;
    entry.hasText = false;
    entry.text[0] = '\0';
    bool foundPort = false;

    while (i < len - 1) {
        uint8_t tag = payload[i++];
        uint8_t field = tag >> 3, wire = tag & 0x07;
        if (wire == 0) {
            uint32_t val = 0;
            int shift = 0;
            while (i < len) {
                uint8_t b = payload[i++];
                val |= (uint32_t)(b & 0x7F) << shift;
                if (!(b & 0x80)) break;
                shift += 7;
            }
            if (field == 1) {
                entry.portNum = val;
                foundPort = true;
            }
        } else if (wire == 1) {
            i += 8;
        } else if (wire == 5) {
            i += 4;
        } else if (wire == 2) {
            if (i >= len) break;
            uint32_t slen = 0;
            int shift = 0;
            while (i < len) {
                uint8_t b = payload[i++];
                slen |= (uint32_t)(b & 0x7F) << shift;
                if (!(b & 0x80)) break;
                shift += 7;
            }
            if (slen > (uint32_t)(len - i)) break;
            
            if (field == 2 && slen > 0) {
                if (entry.portNum == 1 && slen < sizeof(entry.text)) {
                    memcpy(entry.text, &payload[i], slen);
                    entry.text[slen] = '\0';
                    entry.hasText = true;
                } else {
                    entry.hasText = true;
                    // Provide hex dump of payload for debugging apps
                    int maxBytes = (slen < (sizeof(entry.text) / 3 - 2)) ? slen : (sizeof(entry.text) / 3 - 2);
                    char* p = entry.text;
                    for (int k = 0; k < maxBytes; k++) {
                        p += sprintf(p, "%02X ", payload[i + k]);
                    }
                    if (slen > maxBytes) sprintf(p, "...");
                }
            }
            i += slen;
        } else {
            break; // unknown wire type
        }
    }
    return foundPort; // If we parsed a portnum, we likely decoded valid cleartext Data protobuf
}


void decoder_short_press() {
    if (totalPkts == 0) return;
    if (detailMode) {
        payloadPage++;
        drawLog();
        return;
    }
    viewOffset++;
    int maxOffset = (totalPkts < LOG_SIZE) ? totalPkts : LOG_SIZE;
    if (viewOffset >= maxOffset) {
        viewOffset = 0; // Wrap around to Live view (newest)
    }
    drawLog();
}

void decoder_double_press() {
    if (totalPkts == 0) return;
    detailMode = !detailMode;
    payloadPage = 0;
    drawLog();
}

static void drawLog() {
    display_clear();

#if HAS_OLED
    display_draw_text_abs(10, 0, DISPLAY_CYAN, "Meshtastic Decoder");
    display_draw_hline(0, 10, 128, DISPLAY_GRAY);
#else
    display_draw_text_line(30, 15, DISPLAY_CYAN, "Meshtastic Decoder");
#endif

    if (totalPkts == 0) {
#if HAS_OLED
        display_draw_text_abs(5, 30, DISPLAY_CYAN, "Waiting for packets...");
#else
        display_draw_text_line(20, 55, DISPLAY_CYAN, "Waiting for packets...");
        display_draw_text_line(0, 75, DISPLAY_BLACK, "");
        display_draw_text_line(0, 95, DISPLAY_BLACK, "");
        display_draw_text_small_line(0, 125, DISPLAY_BLACK, "");
#endif
        display_draw_hline(0, 20, 240, DISPLAY_GRAY);
        display_update_buffer();
        return;
    }

    int idx = (logHead - 1 - viewOffset + LOG_SIZE * 2) % LOG_SIZE;
    LogEntry& e = log_buf[idx];
    char line[64];

    if (detailMode) {
        int maxPkt = totalPkts < LOG_SIZE ? totalPkts : LOG_SIZE;
        uint32_t age = (millis() - e.timeMs) / 1000;

#if HAS_OLED
        int MAX_CHARS_PER_LINE = 21;
        int MAX_LINES = 3;
        int startY = 34;
        int lineH = 10;

        snprintf(line, sizeof(line), "Msg %d/%d (%lus)", viewOffset + 1, maxPkt, age);
        display_draw_text_small_abs(0, 14, DISPLAY_YELLOW, line);
        if (!e.isCleartext) {
            display_draw_text_small_abs(0, 24, DISPLAY_WHITE, "<Encrypted/Unknown>");
        } else {
            snprintf(line, sizeof(line), "<%s> P%d/%d", getPortName(e.portNum), payloadPage + 1,
                     e.hasText ? ((strlen(e.text) + MAX_CHARS_PER_LINE * MAX_LINES - 1) / (MAX_CHARS_PER_LINE * MAX_LINES)) : 1);
            display_draw_text_small_abs(0, 24, DISPLAY_CYAN, line);
        }
#else
        int MAX_CHARS_PER_LINE = 39;
        int MAX_LINES = 5;
        int startY = 60;
        int lineH = 12;

        snprintf(line, sizeof(line), "Message %d of %d  (-%lus)", viewOffset + 1, maxPkt, age);
        display_draw_text_line(0, 30, DISPLAY_YELLOW, line);
        if (!e.isCleartext) {
            display_draw_text_small_line(0, 48, DISPLAY_WHITE, "<Encrypted / Unknown Data>");
        } else {
            int textLen = e.hasText ? strlen(e.text) : 0;
            int charsPerPage = MAX_CHARS_PER_LINE * MAX_LINES;
            int totalPages = e.hasText ? ((textLen + charsPerPage - 1) / charsPerPage) : 1;
            if (totalPages == 0) totalPages = 1;
            if (payloadPage >= totalPages) payloadPage = 0;
            snprintf(line, sizeof(line), "<%s Packet>  [Page %d/%d]", getPortName(e.portNum), payloadPage + 1, totalPages);
            display_draw_text_small_line(0, 48, DISPLAY_CYAN, line);
        }
#endif

        int textLen = e.hasText ? strlen(e.text) : 0;
        int charsPerPage = MAX_CHARS_PER_LINE * MAX_LINES;
        int totalPages = e.hasText ? ((textLen + charsPerPage - 1) / charsPerPage) : 1;
        if (totalPages == 0) totalPages = 1;
        if (payloadPage >= totalPages) payloadPage = 0;

        for (int i = 0; i < MAX_LINES; i++) {
            int lineStart = payloadPage * charsPerPage + i * MAX_CHARS_PER_LINE;
            if (e.hasText && lineStart < textLen) {
                int len = textLen - lineStart;
                if (len > MAX_CHARS_PER_LINE) len = MAX_CHARS_PER_LINE;
                char lineBuf[42];
                memcpy(lineBuf, e.text + lineStart, len);
                lineBuf[len] = '\0';
#if HAS_OLED
                display_draw_text_small_abs(0, startY + i * lineH, DISPLAY_WHITE, lineBuf);
#else
                display_draw_text_small_line(0, startY + i * lineH, DISPLAY_WHITE, lineBuf);
#endif
            } else {
#if !HAS_OLED
                display_draw_text_small_line(0, startY + i * lineH, DISPLAY_BLACK, "");
#endif
            }
        }
    } else {
        int maxPkt = totalPkts < LOG_SIZE ? totalPkts : LOG_SIZE;
        uint32_t age = (millis() - e.timeMs) / 1000;
#if HAS_OLED
        snprintf(line, sizeof(line), "Pkt %d/%d (%lus) %s", viewOffset + 1, maxPkt, age, (viewOffset == 0) ? "[LIVE]" : "");
        display_draw_text_small_abs(0, 14, DISPLAY_GREEN, line);
        snprintf(line, sizeof(line), "Frm: %08lX", e.srcNode);
        display_draw_text_small_abs(0, 23, DISPLAY_WHITE, line);
        snprintf(line, sizeof(line), "To:  %08lX", e.destNode);
        display_draw_text_small_abs(0, 32, DISPLAY_WHITE, line);
        snprintf(line, sizeof(line), "R:%d S:%d H:%d", (int)e.rssi, (int)e.snr, e.hopLimit);
        display_draw_text_small_abs(0, 41, DISPLAY_YELLOW, line);
        if (e.isCleartext) {
            snprintf(line, sizeof(line), "Port:%s (DblClk)", getPortName(e.portNum));
            display_draw_text_small_abs(0, 50, DISPLAY_CYAN, line);
        } else {
            display_draw_text_small_abs(0, 50, DISPLAY_GRAY, "Encrypted (DblClk)");
        }
#else
        snprintf(line, sizeof(line), "Packet %d / %d  (-%lus)  %s", viewOffset + 1, maxPkt, age, (viewOffset == 0) ? "[LIVE UPDATE]" : "[PAUSED]");
        display_draw_text_line(0, 32, DISPLAY_GREEN, line);
        snprintf(line, sizeof(line), "From: %08lX -> %08lX", e.srcNode, e.destNode);
        display_draw_text_line(0, 55, DISPLAY_WHITE, line);
        snprintf(line, sizeof(line), "Hop:%d   RSSI:%d   SNR:%d", e.hopLimit, (int)e.rssi, (int)e.snr);
        display_draw_text_line(0, 78, DISPLAY_YELLOW, line);
        if (e.isCleartext) {
            snprintf(line, sizeof(line), "Port: %s >> [Dbl-Click]", getPortName(e.portNum));
            display_draw_text_line(0, 101, DISPLAY_CYAN, line);
        } else {
            display_draw_text_line(0, 101, DISPLAY_GRAY, "Payload: Encrypted >> [Dbl-Click]");
        }
        display_draw_text_small_line(0, 125, DISPLAY_GRAY, "Single-Clk: Paging  |  Double-Clk: Read Message");
#endif
    }

#if !HAS_OLED
    display_draw_hline(0, 20, 240, DISPLAY_GRAY);
#endif
    display_update_buffer();
}

void decoder_enter() {
    logHead = 0; totalPkts = 0;
    viewOffset = 0;
    detailMode = false;
    memset(log_buf, 0, sizeof(log_buf));
    lora_apply_channel(0);
    lora_start_listen();
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
    
    bool isCleartext = false;
    if (len > 16) {
        // Try as cleartext first
        if (parseDataProto(buf + 16, len - 16, entry)) {
            isCleartext = true;
        } else {
            // Assume encrypted, try to decrypt with DEFAULT_PSK
            CTR<AES128> ctr;
            uint8_t iv[16] = {0};
            memcpy(iv, &hdr.packetId, 4);
            memcpy(iv + 8, &hdr.srcNode, 4);
            
            ctr.setKey(DEFAULT_PSK, 16);
            ctr.setIV(iv, 16);
            
            uint8_t decrypted[256];
            int dec_len = len - 16;
            if (dec_len > sizeof(decrypted)) dec_len = sizeof(decrypted);
            
            ctr.decrypt(decrypted, buf + 16, dec_len);
            
            // Now parse the decrypted data
            if (parseDataProto(decrypted, dec_len, entry)) {
                isCleartext = true;  // Mark as successfully interpreted
            }
        }
    }
    entry.isCleartext = isCleartext;

    addLog(entry);
    nodetracker_update(hdr.srcNode, entry.rssi, entry.snr);
    drawLog();

    Serial.println("--- Meshtastic Packet ---");
    Serial.print("  From:    0x"); Serial.println(hdr.srcNode, HEX);
    Serial.print("  To:      0x"); Serial.println(hdr.destNode, HEX);
    Serial.print("  HopLim:  ");   Serial.println(hopLimit);
    Serial.print("  RSSI:    ");   Serial.print(entry.rssi);   Serial.println(" dBm");
    Serial.print("  SNR:     ");   Serial.print(entry.snr);    Serial.println(" dB");
    if (entry.isCleartext) {
        Serial.print("  Port:    "); Serial.print(getPortName(entry.portNum));
        Serial.print(" ("); Serial.print(entry.portNum); Serial.println(")");
    }
    if (entry.hasText) { Serial.print("  Payld:   \""); Serial.print(entry.text); Serial.println("\""); }
    Serial.print("  Raw["); Serial.print(len); Serial.print("]: ");
    for (int i = 0; i < len && i < 32; i++) {
        if (buf[i] < 0x10) Serial.print('0');
        Serial.print(buf[i], HEX); Serial.print(' ');
    }
    if (len > 32) Serial.print("...");
    Serial.println();
}
