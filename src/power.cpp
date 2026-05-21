/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "power.h"
#include "pins.h"
#include "button.h"
#include "display.h"
#include "neopixel.h"
#include <Arduino.h>

#if defined(HELTEC_T114)
#include "nrf_gpio.h"
#include "nrf_power.h"
#include "nrf_soc.h"    // sd_power_system_off()
#include "nrf_sdm.h"    // sd_softdevice_is_enabled()
#endif

#if defined(HELTEC_V3)
#include "esp_sleep.h"
#endif

// -----------------------------------------------------------------------
// Shared display helpers
// -----------------------------------------------------------------------

static void show_poweroff_overlay(uint32_t remaining_s) {
#if HAS_OLED
    display_fill_rect_abs(0, 54, 128, 10, DISPLAY_BLACK);
    char buf[22];
    snprintf(buf, sizeof(buf), "PWR OFF in %lus      ", remaining_s);
    display_draw_text_small_abs(0, 62, DISPLAY_WHITE, buf);
    display_update_buffer();
#else
    display_fill_rect_abs(0, 119, 240, 16, DISPLAY_BLACK);
    char buf[36];
    snprintf(buf, sizeof(buf), "  HOLD TO POWER OFF: %lus  ", remaining_s);
    display_draw_text_small_abs(0, 127, DISPLAY_WHITE, buf);
#endif
}

static void clear_poweroff_overlay() {
#if HAS_OLED
    display_fill_rect_abs(0, 54, 128, 10, DISPLAY_BLACK);
    display_update_buffer();
#else
    display_fill_rect_abs(0, 119, 240, 16, DISPLAY_BLACK);
#endif
}

#if defined(ARDUINO_ARCH_ESP32)
static void show_wakeup_progress(uint32_t held_ms) {
    // Green NeoPixels build up over BTN_WAKE_MS to signal hold-to-wake progress.
    uint8_t bright = (uint8_t)((held_ms * 80) / BTN_WAKE_MS);
    if (bright > 80) bright = 80;
    neopixel_set_color(0, bright, 0);
}
#endif

static bool was_showing_overlay = false;

// -----------------------------------------------------------------------
// Platform: nRF52840 (T114)
// -----------------------------------------------------------------------
#if defined(HELTEC_T114)

static bool woke_from_gpio() {
    return (NRF_POWER->RESETREAS & POWER_RESETREAS_OFF_Msk) != 0;
}

static void enter_system_off() {
    // CRITICAL: If the button is LOW when SENSE_LOW is configured, the DETECT
    // signal is asserted immediately and the device wakes before entering
    // System OFF. The bootloader then sees a held button and enters BLE OTA.
    // Wait for the button to be released (with a short timeout for safety).
    uint32_t t = millis();
    while (digitalRead(PIN_BUTTON) == LOW && (millis() - t) < 3000) {
        delay(10);
    }
    delay(50);  // debounce

    // Set GPREGRET = 0x6d (DFU_MAGIC_SKIP) before entering System OFF.
    // The Adafruit/Heltec bootloader checks this register on every reset:
    // - 0x6d  → skip DFU mode check, boot the application normally
    // - 0x00  → default; bootloader may enter BLE OTA if it detects a trigger
    // Without this, waking from System OFF always entered BLE OTA DFU mode.
    // Reference: Meshtastic firmware cpuDeepSleep(), SoftRF nRF52 platform.
    uint8_t sd_enabled = 0;
    sd_softdevice_is_enabled(&sd_enabled);
    if (sd_enabled) {
        sd_power_gpregret_clr(0, 0xFF);
        sd_power_gpregret_set(0, 0x6d);   // DFU_MAGIC_SKIP
    } else {
        NRF_POWER->GPREGRET = 0x6d;
    }

    // Configure button pin to sense LOW -- now safe because button is released.
    nrf_gpio_cfg_sense_input(
        g_ADigitalPinMap[PIN_BUTTON],
        NRF_GPIO_PIN_PULLUP,
        NRF_GPIO_PIN_SENSE_LOW
    );

    // Use the SoftDevice API if running -- direct SYSTEMOFF access while the
    // SD is active causes a fault and the bootloader enters BLE DFU recovery.
    if (sd_enabled) {
        sd_power_system_off();
    } else {
        NRF_POWER->SYSTEMOFF = 1;
    }
    while (1) {}  // unreachable
}

void power_standby() {
    Serial.println("Entering standby...");
    Serial.flush();
    delay(200);

    display_off();
    neopixel_off();
    digitalWrite(PIN_GPS_VEXT, LOW);  // cut GPS power

    enter_system_off();
}

void power_init() {
    // NOTE: On nRF52840 the Heltec/Adafruit bootloader runs BEFORE this code
    // after any reset (including GPIO wakeup from System OFF). The bootloader
    // monitors the user button: if it is held LOW at boot it enters BLE OTA DFU
    // mode and our firmware never starts. Therefore we cannot require a button
    // hold here -- the user must press and release to wake, allowing the
    // bootloader to see the button released and pass control to the application.
    //
    // Wake behaviour: any brief button press wakes the device. Normal boot
    // follows immediately. The 10-second hold-to-sleep is still available.
    bool gpio_wakeup = woke_from_gpio();
    NRF_POWER->RESETREAS = 0xFFFFFFFF;  // clear reason

    if (!gpio_wakeup) return;  // normal power-on, nothing special to do

    // Woke from standby -- just let the normal setup() proceed.
    // A green NeoPixel flash gives brief visual confirmation of the wakeup.
    neopixel_init();
    neopixel_set_color(0, 80, 0);
    delay(400);
    neopixel_off();
}

// -----------------------------------------------------------------------
// Platform: ESP32-S3 (V3)
// -----------------------------------------------------------------------
#elif defined(HELTEC_V3)

void power_standby() {
    Serial.println("Entering standby...");
    Serial.flush();
    delay(200);

    display_off();
    neopixel_off();

    // Wake on button (GPIO 0) pulled LOW
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTON, 0);
    esp_deep_sleep_start();
}

void power_init() {
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;

    neopixel_init();

    uint32_t t = millis();
    bool awake = false;
    while (digitalRead(PIN_BUTTON) == LOW) {
        uint32_t held = millis() - t;
        show_wakeup_progress(held);
        if (held >= BTN_WAKE_MS) { awake = true; break; }
        delay(40);
    }

    neopixel_off();

    if (!awake) {
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTON, 0);
        esp_deep_sleep_start();
    }
}

// -----------------------------------------------------------------------
// Stub for unsupported platforms
// -----------------------------------------------------------------------
#else
void power_standby() { Serial.println("Standby not supported."); }
void power_init()    {}
#endif

// -----------------------------------------------------------------------
// Common: called every loop() -- countdown overlay + power-off trigger
// -----------------------------------------------------------------------
void power_update() {
    // button_poweroff_pressed() is already checked in main.cpp before this,
    // but handle it here too as a safety net
    if (button_poweroff_pressed()) {
        power_standby();
        return;
    }

    uint32_t held = button_held_ms();

    if (held >= BTN_WARN_MS) {
        static uint32_t lastUpdate = 0;
        uint32_t now = millis();
        if (!was_showing_overlay || now - lastUpdate >= 500) {
            lastUpdate = now;
            was_showing_overlay = true;
            uint32_t remaining = (BTN_POWEROFF_MS - held + 999) / 1000;
            show_poweroff_overlay(remaining);
            neopixel_set_color(60, 30, 0);  // amber warning
        }
    } else if (was_showing_overlay) {
        was_showing_overlay = false;
        clear_poweroff_overlay();
    }
}
