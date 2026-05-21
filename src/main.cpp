/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include <Arduino.h>
#include "pins.h"
#include "modes.h"
#include "display.h"
#include "gps.h"
#include "lora.h"
#include "shell.h"
#include "button.h"
#include "menu.h"
#include "neopixel.h"
#include "power.h"
#include "spectrum.h"
#include "waterfall.h"
#include "scanner.h"
#include "monitor.h"
#include "decoder.h"
#include "nodetracker.h"
#include "noise.h"
#include "stats.h"
#include "autotrack.h"
#include "ble_ota.h"

AppMode currentMode = MODE_MENU;

static uint32_t lastStatusUpdate  = 0;
static uint32_t lastNeopixelUpdate = 0;

static float read_battery_voltage() {
    pinMode(PIN_BAT_ADC_EN, OUTPUT);
    digitalWrite(PIN_BAT_ADC_EN, HIGH);
    delay(5);
    int raw = analogRead(PIN_BAT_ADC);
    digitalWrite(PIN_BAT_ADC_EN, LOW);
    return (raw / 1024.0f) * 3.6f * BAT_ADC_MULTIPLIER;
}

const char* mode_name(AppMode m) {
    switch (m) {
        case MODE_STATUS:   return "Status";
        case MODE_SPECTRUM: return "Spectrum";
        case MODE_WATERFALL:return "Waterfall";
        case MODE_NOISE:    return "Noise Floor";
        case MODE_SCANNER:  return "Scanner";
        case MODE_MONITOR:  return "Monitor";
        case MODE_DECODER:  return "Decoder";
        case MODE_NODES:    return "Nodes";
        case MODE_STATS:    return "Stats";
        case MODE_AUTOTRACK:return "AutoTrack";
        case MODE_OTA:      return "OTA Update";
        case MODE_STANDBY:  return "Standby";
        default:            return "Menu";
    }
}

static void enter_mode(AppMode m) {
    currentMode = m;
    display_clear(true);
    switch (m) {
        case MODE_MENU:      menu_init(); menu_draw(); break;
        case MODE_STATUS:    lora_start_listen(); break;
        case MODE_SPECTRUM:  lora_stop_listen(); spectrum_enter(); break;
        case MODE_WATERFALL: lora_stop_listen(); waterfall_enter(); break;
        case MODE_NOISE:     lora_stop_listen(); noise_enter(); break;
        case MODE_SCANNER:   lora_stop_listen(); scanner_enter();  break;
        case MODE_MONITOR:   monitor_enter();  break;
        case MODE_DECODER:   decoder_enter();  break;
        case MODE_NODES:     nodetracker_enter(); break;
        case MODE_STATS:     stats_enter(); lora_start_listen(); break;
        case MODE_AUTOTRACK: autotrack_enter(); break;
        case MODE_OTA:       lora_stop_listen(); ble_ota_enter(); break;
        case MODE_STANDBY: {
            // Show shutdown screen briefly before entering deep sleep
            display_clear();
#if HAS_OLED
            display_draw_text_abs(25, 0, DISPLAY_CYAN, "Entering Standby");
            display_draw_hline(0, 10, 128, DISPLAY_GRAY);
            display_draw_text_small_abs(10, 25, DISPLAY_WHITE, "Press button");
            display_draw_text_small_abs(18, 35, DISPLAY_WHITE, "to wake up.");
            display_update_buffer();
#else
            display_draw_text_abs(40, 50, DISPLAY_CYAN, "Entering Standby");
            display_draw_text_abs(30, 80, DISPLAY_WHITE, "Press button to wake.");
#endif
            neopixel_set_color(60, 0, 0);   // red -- going to sleep
            lora_stop_listen();
            delay(2000);
            power_standby();
            break;
        }
        default: break;
    }
}

void setup() {
    // Must be first -- may not return if woken from standby and button released early
    power_init();

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LED_STATE_OFF);

    shell_init();
    button_init();
    neopixel_init();

    Serial.println("Initializing display...");
    display_init();

    Serial.println("Initializing GPS...");
    gps_init();

    Serial.println("Initializing LoRa...");
    lora_init();

    for (int i = 0; i < 3; i++) {
        digitalWrite(PIN_LED, LED_STATE_ON);  delay(100);
        digitalWrite(PIN_LED, LED_STATE_OFF); delay(100);
    }

    enter_mode(MODE_MENU);
    Serial.println("Ready. Short=next Long=select/back");
    Serial.print("> ");
}

void loop() {
    gps_update();
    lora_update();
    shell_update();
    button_update();

    // Power management -- handles 10s poweroff and countdown overlay.
    // Must come before neopixel_update so amber warning can override status colours.
    power_update();

    // Normal neopixel status (only when not in power-off warning zone)
    uint32_t now = millis();
    if (button_held_ms() < BTN_WARN_MS && now - lastNeopixelUpdate > 100) {
        lastNeopixelUpdate = now;
        bool rxRecent = (now - lora_last_rx_ms()) < 300;
        neopixel_update(gps_has_fix(), rxRecent);
    }

    if (lora_poll_packet()) {
        uint8_t buf[256]; int len = 0;
        lora_get_last_packet(buf, &len);
        if (len >= 8) {
            uint32_t srcNode; memcpy(&srcNode, buf + 4, 4);
            stats_record_packet(srcNode, lora_last_rssi());
        }
        lora_requeue_packet();
    }

    if (button_long_pressed()) {
        if (currentMode == MODE_MENU) {
            enter_mode(menu_selection());
        } else {
            lora_stop_listen();
            enter_mode(MODE_MENU);
        }
        return;
    }

    if (currentMode == MODE_MENU && (button_short_pressed() || button_double_pressed())) {
        menu_update();
        menu_draw();
        return;
    }

    if (currentMode == MODE_NODES && button_short_pressed()) {
        nodetracker_mode_update();
        return;
    }

    if (currentMode == MODE_SCANNER && button_short_pressed()) {
        scanner_short_press();
        return;
    }

    if (currentMode == MODE_SPECTRUM && button_short_pressed()) {
        spectrum_short_press();
        return;
    }

    if (currentMode == MODE_WATERFALL) {
        if (button_double_pressed()) {
            waterfall_double_press();
            return;
        }
        if (button_short_pressed()) {
            waterfall_short_press();
            return;
        }
    }

    if (currentMode == MODE_NOISE && button_short_pressed()) {
        noise_short_press();
        return;
    }

    if (currentMode == MODE_DECODER) {
        if (button_double_pressed()) {
            decoder_double_press();
            return;
        }
        if (button_short_pressed()) {
            decoder_short_press();
            return;
        }
    }

    if (currentMode == MODE_STATS) {
        if (button_short_pressed()) {
            stats_short_press();
            return;
        }
    }

    switch (currentMode) {
        case MODE_STATUS: {
            if (now - lastStatusUpdate >= 1000) {
                lastStatusUpdate = now;
                display_update(
                    gps_latitude(), gps_longitude(),
                    gps_satellites(), gps_has_fix(),
                    lora_last_rssi(), lora_last_snr(),
                    lora_packet_count(), lora_is_listening(),
                    read_battery_voltage(), now / 1000
                );
            }
            break;
        }
        case MODE_SPECTRUM:  spectrum_update();           break;
        case MODE_WATERFALL: waterfall_update();          break;
        case MODE_NOISE:     noise_update();              break;
        case MODE_SCANNER:   scanner_update();            break;
        case MODE_MONITOR:   monitor_update();            break;
        case MODE_DECODER:   decoder_update();            break;
        case MODE_NODES:     nodetracker_mode_update();   break;
        case MODE_STATS:     stats_update();              break;
        case MODE_AUTOTRACK: autotrack_update();          break;
        case MODE_OTA:       ble_ota_update();            break;
        default: break;
    }

#if HAS_OLED
    if (currentMode != MODE_STATUS && currentMode != MODE_MENU) {
        display_update_buffer();
    }
#endif
}
