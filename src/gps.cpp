/*
 * Copyright (c) 2026 Martin Bogomolni <martinbogo@gmail.com>
 *
 * This code is licensed under the Creative Commons 
 * Attribution-NonCommercial-NoDerivatives 4.0 International License (CC BY-NC-ND 4.0).
 * To view a copy of this license, visit:
 * http://creativecommons.org/licenses/by-nc-nd/4.0/
 */

#include "gps.h"
#include "pins.h"

#if HAS_GPS

#include <TinyGPSPlus.h>

static TinyGPSPlus parser;

void gps_init() {
    pinMode(PIN_GPS_VEXT, OUTPUT);
#if defined(HELTEC_V3) || defined(HELTEC_V4)
    digitalWrite(PIN_GPS_VEXT, LOW);    // Active LOW on ESP32 Heltec V3 and V4
#else
    digitalWrite(PIN_GPS_VEXT, HIGH);   // Active HIGH on T114
#endif

    if (PIN_GPS_STANDBY >= 0) {
        pinMode(PIN_GPS_STANDBY, OUTPUT);
        digitalWrite(PIN_GPS_STANDBY, HIGH); // keep GPS awake
    }

#ifdef PIN_GPS_RST
    pinMode(PIN_GPS_RST, OUTPUT);
    digitalWrite(PIN_GPS_RST, LOW);
    delay(150);
    digitalWrite(PIN_GPS_RST, HIGH);
#endif

    delay(1000); // L76K needs warmup after power-on

#if defined(ESP32)
    // ESP32 requires explicit pin routing for Serial1 since variants might not match
    Serial1.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
#else
    // Serial1 uses PIN_SERIAL1_RX/TX from variant.h (GPS UART pins)
    Serial1.begin(9600);
#endif
}

void gps_update() {
    while (Serial1.available()) {
        parser.encode(Serial1.read());
    }
}

float gps_latitude() {
    return parser.location.isValid() ? (float)parser.location.lat() : 0.0f;
}

float gps_longitude() {
    return parser.location.isValid() ? (float)parser.location.lng() : 0.0f;
}

float gps_altitude() {
    return parser.altitude.isValid() ? (float)parser.altitude.meters() : 0.0f;
}

float gps_speed_kmh() {
    return parser.speed.isValid() ? (float)parser.speed.kmph() : 0.0f;
}

int gps_satellites() {
    return parser.satellites.isValid() ? parser.satellites.value() : 0;
}

bool gps_has_fix() {
    return parser.location.isValid() && parser.location.age() < 3000;
}

uint32_t gps_chars_processed() {
    return parser.charsProcessed();
}

#else

// Stub implementations for boards without GPS
void gps_init() {}
void gps_update() {}
float gps_latitude() { return 0.0f; }
float gps_longitude() { return 0.0f; }
float gps_altitude() { return 0.0f; }
float gps_speed_kmh() { return 0.0f; }
int gps_satellites() { return 0; }
bool gps_has_fix() { return false; }
uint32_t gps_chars_processed() { return 0; }

#endif

void test_baud_rates_and_pins(int rx, int tx, const char* label) {
    long bauds[] = {9600, 38400, 115200};
    for (int b = 0; b < 3; b++) {
        Serial.printf("\nTesting %s at %ld baud...\n", label, bauds[b]);
        Serial1.end();
        delay(10);
        Serial1.begin(bauds[b], SERIAL_8N1, rx, tx);
        delay(100);
        
        while(Serial1.available()) Serial1.read(); // flush
        
        // Let it just listen for a bit
        unsigned long start = millis();
        int received = 0;
        while(millis() - start < 1500) {
            if(Serial1.available()) {
                char c = Serial1.read();
                if (c >= 32 && c <= 126) Serial.print(c);
                else Serial.printf("<%02X>", c);
                received++;
            }
        }
        
        if (received == 0) {
            // try prompting
            Serial1.println("$PMTK605*31");
            start = millis();
            int r2 = 0;
            while(millis() - start < 1000) {
                 if(Serial1.available()) {
                    char c = Serial1.read();
                    if (c >= 32 && c <= 126) Serial.print(c);
                    else Serial.printf("<%02X>", c);
                    received++;
                    r2++;
                }
            }
        }
        
        Serial.printf("\nResult: %d bytes rx'd\n", received);
        if (received > 5) break; 
    }
}

void gps_diagnostic_test() {
    Serial.println("\n--- Deep GPS Diagnostic ---");
    Serial.println("Powering off GPS (VEXT=HIGH)...");
#ifdef PIN_GPS_VEXT
    pinMode(PIN_GPS_VEXT, OUTPUT);
    digitalWrite(PIN_GPS_VEXT, HIGH);
#endif
    delay(500);

    Serial.println("Powering on GPS (VEXT=LOW)...");
#ifdef PIN_GPS_VEXT
    digitalWrite(PIN_GPS_VEXT, LOW);
#endif
    delay(500);

    Serial.println("Asserting STANDBY (40) HIGH (Wake)...");
#ifdef PIN_GPS_STANDBY
    pinMode(PIN_GPS_STANDBY, OUTPUT);
    digitalWrite(PIN_GPS_STANDBY, HIGH);
#endif

    Serial.println("Resetting GPS (42): LOW 200ms -> HIGH...");
#ifdef PIN_GPS_RST
    pinMode(PIN_GPS_RST, OUTPUT);
    digitalWrite(PIN_GPS_RST, LOW);
    delay(200);
    digitalWrite(PIN_GPS_RST, HIGH);
#endif
    delay(1000); // boot time

#if defined(PIN_GPS_RX) && defined(PIN_GPS_TX)
    test_baud_rates_and_pins(PIN_GPS_RX, PIN_GPS_TX, "Standard (RX=38, TX=39)");
    
    // Check if we received anything, if not, try swapped pins
    test_baud_rates_and_pins(PIN_GPS_TX, PIN_GPS_RX, "Swapped (RX=39, TX=38)");
    
    // Just in case it's active HIGH for power:
    Serial.println("\n--- Let's try VEXT=HIGH (just in case)...");
#ifdef PIN_GPS_VEXT
    digitalWrite(PIN_GPS_VEXT, HIGH);
#endif
    delay(1000);
    test_baud_rates_and_pins(PIN_GPS_RX, PIN_GPS_TX, "Standard (RX=38, TX=39)");
#else
    Serial.println("PIN_GPS_RX or PIN_GPS_TX not defined!");
#endif
    
    Serial.println("\n--- End Diagnostic ---");
}
