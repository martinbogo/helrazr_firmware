/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "shell.h"
#include "pins.h"
#include "display.h"
#include "gps.h"
#include "lora.h"

static char linebuf[128];
static int linepos = 0;

static float read_battery() {
    pinMode(PIN_BAT_ADC_EN, OUTPUT);
    digitalWrite(PIN_BAT_ADC_EN, HIGH);
    delay(5);
    int raw = analogRead(PIN_BAT_ADC);
    digitalWrite(PIN_BAT_ADC_EN, LOW);
    return (raw / 1024.0f) * 3.6f * BAT_ADC_MULTIPLIER;
}

static void print_help() {
    Serial.println("Commands:");
    Serial.println("  help              Show this help");
    Serial.println("  status            Show all status");
    Serial.println("  gps               GPS info");
    Serial.println("  lora              LoRa info");
    Serial.println("  lora listen       Start receiving");
    Serial.println("  lora stop         Stop receiving");
    Serial.println("  lora freq <MHz>   Set frequency");
    Serial.println("  lora bw <kHz>     Set bandwidth");
    Serial.println("  lora sf <7-12>    Set spreading factor");
    Serial.println("  display on        Turn display on");
    Serial.println("  display off       Turn display off");
    Serial.println("  led on|off        Toggle LED");
    Serial.println("  bat               Battery voltage");
    Serial.println("  reboot            Software reset");
}

static void cmd_status() {
    Serial.println("--- GPS ---");
    Serial.print("  Fix:  "); Serial.println(gps_has_fix() ? "3D" : "No");
    Serial.print("  Sats: "); Serial.println(gps_satellites());
    Serial.print("  Lat:  "); Serial.println(gps_latitude(), 6);
    Serial.print("  Lon:  "); Serial.println(gps_longitude(), 6);
    Serial.print("  Alt:  "); Serial.print(gps_altitude(), 1); Serial.println(" m");
    Serial.print("  Chars:"); Serial.println(gps_chars_processed());
    Serial.println("--- LoRa ---");
    Serial.print("  Mode: "); Serial.println(lora_is_listening() ? "RX" : "Idle");
    Serial.print("  Freq: "); Serial.print(lora_frequency(), 1); Serial.println(" MHz");
    Serial.print("  BW:   "); Serial.print(lora_bandwidth(), 0); Serial.println(" kHz");
    Serial.print("  SF:   "); Serial.println(lora_spreading_factor());
    Serial.print("  RSSI: "); Serial.print(lora_last_rssi(), 1); Serial.println(" dBm");
    Serial.print("  SNR:  "); Serial.print(lora_last_snr(), 1); Serial.println(" dB");
    Serial.print("  Pkts: "); Serial.println(lora_packet_count());
    Serial.println("--- System ---");
    Serial.print("  Bat:  "); Serial.print(read_battery(), 2); Serial.println(" V");
    Serial.print("  Up:   "); Serial.print(millis() / 1000); Serial.println(" s");
    Serial.print("  Disp: "); Serial.println(display_is_on() ? "On" : "Off");
}

static void cmd_gps() {
    Serial.print("Fix:   "); Serial.println(gps_has_fix() ? "3D" : "No");
    Serial.print("Sats:  "); Serial.println(gps_satellites());
    Serial.print("Lat:   "); Serial.println(gps_latitude(), 6);
    Serial.print("Lon:   "); Serial.println(gps_longitude(), 6);
    Serial.print("Alt:   "); Serial.print(gps_altitude(), 1); Serial.println(" m");
    Serial.print("Speed: "); Serial.print(gps_speed_kmh(), 1); Serial.println(" km/h");
    Serial.print("Chars: "); Serial.println(gps_chars_processed());
}

static void cmd_lora() {
    Serial.print("Mode: "); Serial.println(lora_is_listening() ? "RX" : "Idle");
    Serial.print("Freq: "); Serial.print(lora_frequency(), 1); Serial.println(" MHz");
    Serial.print("BW:   "); Serial.print(lora_bandwidth(), 0); Serial.println(" kHz");
    Serial.print("SF:   "); Serial.println(lora_spreading_factor());
    Serial.print("RSSI: "); Serial.print(lora_last_rssi(), 1); Serial.println(" dBm");
    Serial.print("SNR:  "); Serial.print(lora_last_snr(), 1); Serial.println(" dB");
    Serial.print("Pkts: "); Serial.println(lora_packet_count());
}

static bool startsWith(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

static void process_line(char* line) {
    // trim leading/trailing whitespace
    while (*line == ' ') line++;
    int len = strlen(line);
    while (len > 0 && line[len - 1] == ' ') line[--len] = '\0';
    if (len == 0) return;

    if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
        print_help();
    } else if (strcmp(line, "status") == 0) {
        cmd_status();
    } else if (strcmp(line, "gps") == 0) {
        cmd_gps();
    } else if (strcmp(line, "lora") == 0) {
        cmd_lora();
    } else if (strcmp(line, "lora listen") == 0) {
        if (lora_start_listen()) Serial.println("LoRa: listening");
        else Serial.println("LoRa: failed to start");
    } else if (strcmp(line, "lora stop") == 0) {
        lora_stop_listen();
        Serial.println("LoRa: stopped");
    } else if (startsWith(line, "lora freq ")) {
        float f = atof(line + 10);
        if (lora_set_frequency(f)) {
            Serial.print("LoRa freq: "); Serial.print(f, 1); Serial.println(" MHz");
        } else {
            Serial.println("Failed to set frequency");
        }
    } else if (startsWith(line, "lora bw ")) {
        float bw = atof(line + 8);
        if (lora_set_bandwidth(bw)) {
            Serial.print("LoRa BW: "); Serial.print(bw, 0); Serial.println(" kHz");
        } else {
            Serial.println("Failed to set bandwidth");
        }
    } else if (startsWith(line, "lora sf ")) {
        int sf = atoi(line + 8);
        if (lora_set_spreading_factor(sf)) {
            Serial.print("LoRa SF: "); Serial.println(sf);
        } else {
            Serial.println("SF must be 5-12");
        }
    } else if (strcmp(line, "display on") == 0) {
        display_on();
        Serial.println("Display on");
    } else if (strcmp(line, "display off") == 0) {
        display_off();
        Serial.println("Display off");
    } else if (strcmp(line, "led on") == 0) {
        digitalWrite(PIN_LED, LED_STATE_ON);
        Serial.println("LED on");
    } else if (strcmp(line, "led off") == 0) {
        digitalWrite(PIN_LED, LED_STATE_OFF);
        Serial.println("LED off");
    } else if (strcmp(line, "bat") == 0) {
        Serial.print("Battery: "); Serial.print(read_battery(), 2); Serial.println(" V");
    } else if (strcmp(line, "reboot") == 0) {
        Serial.println("Rebooting...");
        delay(100);
#if defined(HELTEC_T114)
        NVIC_SystemReset();
#elif defined(HELTEC_V3)
        ESP.restart();
#endif
    } else {
        Serial.print("Unknown command: ");
        Serial.println(line);
        Serial.println("Type 'help' for commands");
    }
}

void shell_init() {
    Serial.begin(115200);
    // wait up to 3s for USB serial to connect
    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) {
        delay(10);
    }
    Serial.println();
    Serial.println("=== Heltec T114 Firmware ===");
    Serial.println("Type 'help' for commands");
    Serial.print("> ");
}

void shell_update() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r' || c == '\n') {
            if (linepos > 0) {
                Serial.println();
                linebuf[linepos] = '\0';
                process_line(linebuf);
                linepos = 0;
                Serial.print("> ");
            }
        } else if (c == '\b' || c == 127) {  // backspace/delete
            if (linepos > 0) {
                linepos--;
                Serial.print("\b \b");
            }
        } else if (linepos < (int)sizeof(linebuf) - 1) {
            linebuf[linepos++] = c;
            Serial.print(c);  // echo
        }
    }
}
