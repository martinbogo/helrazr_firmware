#ifndef _VARIANT_HELTEC_T114_
#define _VARIANT_HELTEC_T114_

#define VARIANT_MCK (64000000ul)
#define USE_LFXO

#include "WVariant.h"

#ifdef __cplusplus
extern "C" {
#endif

// Number of pins
#define PINS_COUNT (48)
#define NUM_DIGITAL_PINS (48)
#define NUM_ANALOG_INPUTS (1)
#define NUM_ANALOG_OUTPUTS (0)

// LEDs
#define PIN_LED1 (32 + 3)  // P1.03, green, active low
#define LED_STATE_ON 0

// NeoPixels
#define PIN_NEOPIXEL 14
#define NEOPIXEL_NUM 2

// Button
#define PIN_BUTTON1 (32 + 10)  // P1.10

// UART (unused, but required by BSP)
#define PIN_SERIAL1_RX (32 + 5)  // GPS RX (CPU TX -> GPS)
#define PIN_SERIAL1_TX (32 + 7)  // GPS TX (GPS -> CPU RX)
#define PIN_SERIAL2_RX (0 + 9)
#define PIN_SERIAL2_TX (0 + 10)

// I2C bus 0 (RTC footprint, unpopulated)
#define PIN_WIRE_SDA (0 + 26)
#define PIN_WIRE_SCL (0 + 27)

// I2C bus 1 (header)
#define WIRE_INTERFACES_COUNT 2
#define PIN_WIRE1_SDA (0 + 16)
#define PIN_WIRE1_SCL (0 + 13)

// SPI interfaces
#define SPI_INTERFACES_COUNT 2

// SPI 0 - LoRa
#define PIN_SPI_MISO (0 + 23)
#define PIN_SPI_MOSI (0 + 22)
#define PIN_SPI_SCK  (0 + 19)

// SPI 1 - Display
#define PIN_SPI1_MISO (-1)
#define PIN_SPI1_MOSI (32 + 9)   // P1.09
#define PIN_SPI1_SCK  (32 + 8)   // P1.08

// QSPI Flash
#define PIN_QSPI_SCK  (32 + 14)
#define PIN_QSPI_CS   (32 + 15)
#define PIN_QSPI_IO0  (32 + 12)
#define PIN_QSPI_IO1  (32 + 13)
#define PIN_QSPI_IO2  (0 + 7)
#define PIN_QSPI_IO3  (0 + 5)
#define EXTERNAL_FLASH_DEVICES MX25R1635F
#define EXTERNAL_FLASH_USE_QSPI

// Battery
#define BATTERY_PIN (0 + 4)
#define ADC_CTRL (0 + 6)
#define ADC_CTRL_ENABLED HIGH
#define ADC_RESOLUTION 14
#define BATTERY_SENSE_RESOLUTION_BITS 12
#define BATTERY_SENSE_RESOLUTION 4096.0
#undef AREF_VOLTAGE
#define AREF_VOLTAGE 3.0
#define VBAT_AR_INTERNAL AR_INTERNAL_3_0
#define ADC_MULTIPLIER (4.916F)

// Analog pins
#define PIN_A0 (0 + 4)

#ifdef __cplusplus
}
#endif

#endif
