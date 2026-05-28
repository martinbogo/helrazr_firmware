/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "ble_ota.h"
#include "display.h"

#ifdef HELTEC_T114
// --- T114 Implementation (Nordic nRF52) ---
#include <Arduino.h>

void ble_ota_enter() {
    display_clear();
    display_draw_text(0, 15, DISPLAY_RED, "Starting BLE OTA...");
    display_draw_text(0, 30, DISPLAY_WHITE, "Use Bluefruit App");
    display_draw_text(0, 45, DISPLAY_WHITE, "Searching for DfuTarg");
    display_update_buffer();

    delay(2000);
    // Nordic specific macro to reboot into the Secure DFU Bootloader
    extern void enterOTADfu(void);
    enterOTADfu();
}

void ble_ota_update() {
    // Unreachable; MCU resets
}

#else
// --- V3 Implementation (ESP32-S3) ---
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Update.h>

#define SERVICE_UUID           "0000ffff-0000-1000-8000-00805f9b34fb"
#define CHAR_CONTROL_UUID      "0000ff01-0000-1000-8000-00805f9b34fb"
#define CHAR_DATA_UUID         "0000ff02-0000-1000-8000-00805f9b34fb"

static BLECharacteristic* pControlChar = nullptr;
static BLECharacteristic* pDataChar = nullptr;
static bool ota_active = false;
static uint32_t totalBytes = 0;
static uint32_t expectedBytes = 0;

class OtaControlCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        std::string data = pChar->getValue();
        if (data.length() > 0) {
            uint8_t cmd = data[0];
            if (cmd == 0x01 && data.length() >= 5) {
                // START Command: 0x01 + 4 bytes size
                expectedBytes = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
                totalBytes = 0;
                if (!Update.begin(expectedBytes)) {
                    Serial.println("Update begin failed");
                }
                ota_active = true;
            } else if (cmd == 0x02) {
                // END Command
                if (Update.end(true)) {
                    Serial.println("Update Success");
                    delay(500);
                    ESP.restart();
                } else {
                    Serial.println("Update failed");
                }
                ota_active = false;
            }
        }
    }
};

class OtaDataCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        std::string data = pChar->getValue();
        if (ota_active && data.length() > 0) {
            Update.write((uint8_t*)data.c_str(), data.length());
            totalBytes += data.length();
        }
    }
};

void ble_ota_enter() {
    display_clear();
    display_draw_text(0, 15, DISPLAY_CYAN, "BLE OTA Mode");
    display_draw_text_small(0, 30, DISPLAY_WHITE, "Waiting for Host...");
    display_update_buffer();

    BLEDevice::init("Helrazr-OTA");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    pControlChar = pService->createCharacteristic(
        CHAR_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pControlChar->setCallbacks(new OtaControlCallbacks());

    pDataChar = pService->createCharacteristic(
        CHAR_DATA_UUID,
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    pDataChar->setCallbacks(new OtaDataCallbacks());

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();
}

void ble_ota_update() {
    if (ota_active) {
        display_draw_text_line(0, 15, DISPLAY_CYAN, "Flashing...");

        char buf[32];
        if(expectedBytes > 0) {
            int pct = (totalBytes * 100) / expectedBytes;
            snprintf(buf, sizeof(buf), "%d%%  (%lu B)", pct, totalBytes);
        } else {
            snprintf(buf, sizeof(buf), "%lu B", totalBytes);
        }
        display_draw_text_small_line(0, 40, DISPLAY_WHITE, buf);
        display_update_buffer();
    }
}
#endif
