#include "gps.h"
#include "pins.h"

#if HAS_GPS

#include <TinyGPSPlus.h>

static TinyGPSPlus parser;

void gps_init() {
    pinMode(PIN_GPS_VEXT, OUTPUT);
    digitalWrite(PIN_GPS_VEXT, HIGH);   // power on GPS

    pinMode(PIN_GPS_STANDBY, OUTPUT);
    digitalWrite(PIN_GPS_STANDBY, HIGH); // keep GPS awake

    delay(1000); // L76K needs warmup after power-on

    // Serial1 uses PIN_SERIAL1_RX/TX from variant.h (GPS UART pins)
    Serial1.begin(9600);
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
