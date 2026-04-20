/*
 * variant.h - ProMicro NRF52840 + WeAct Epaper 2.13" + E22/E22P
 * Pin-to-GPIO mapping is identical to the promicro variant (same MCU).
 */

#pragma once

#include "WVariant.h"

////////////////////////////////////////////////////////////////////////////////
// Low frequency clock source

#define VARIANT_MCK       (64000000ul)

//#define USE_LFXO      // 32.768 kHz crystal oscillator
#define USE_LFRC    // 32.768 kHz RC oscillator

////////////////////////////////////////////////////////////////////////////////
// Power

#define PIN_EXT_VCC          (21)
#define EXT_VCC              (PIN_EXT_VCC)

#define BATTERY_PIN          (17)
#define ADC_RESOLUTION       12

////////////////////////////////////////////////////////////////////////////////
// Number of pins

#define PINS_COUNT           (23)
#define NUM_DIGITAL_PINS     (23)
#define NUM_ANALOG_INPUTS    (3)
#define NUM_ANALOG_OUTPUTS   (0)

////////////////////////////////////////////////////////////////////////////////
// UART pin definition

#define PIN_SERIAL1_TX       (20)
#define PIN_SERIAL1_RX       (18)

////////////////////////////////////////////////////////////////////////////////
// I2C pin definition (defined for compatibility but Wire not used in this variant)

#define WIRE_INTERFACES_COUNT 1

#define PIN_WIRE_SDA         (6)
#define PIN_WIRE_SCL         (7)

////////////////////////////////////////////////////////////////////////////////
// SPI pin definition
// Note: main SPI pins are overridden at runtime via SPI.setPins() by CustomSX1262
// using P_LORA_SCLK/MOSI/MISO defined in PromicroEinkBoard.h (D3/D4/D5).
// SPI1 (D0/D19/D1) is used for the e-ink display.

#define SPI_INTERFACES_COUNT 2

#define PIN_SPI_SCK          (3)
#define PIN_SPI_MISO         (5)
#define PIN_SPI_MOSI         (4)

#define PIN_SPI_NSS          (13)

#define PIN_SPI1_SCK         (0)
#define PIN_SPI1_MISO        (19)
#define PIN_SPI1_MOSI        (1)

////////////////////////////////////////////////////////////////////////////////
// Builtin LEDs

#define PIN_LED              (22)
#define LED_PIN              PIN_LED
#define LED_BLUE             PIN_LED
#define LED_BUILTIN          PIN_LED
#define LED_STATE_ON         1

////////////////////////////////////////////////////////////////////////////////
// Builtin buttons

#define PIN_BUTTON1          (6)
#define BUTTON_PIN           PIN_BUTTON1

// GxEPD2 requires SCK/MISO/MOSI globals (used by GxEPD2_1248c which is compiled
// even though it's never used on this target).
extern const int SCK;
extern const int MISO;
extern const int MOSI;
