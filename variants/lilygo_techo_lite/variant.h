/*
 * variant.h
 * Copyright (C) 2023 Seeed K.K.
 * MIT License
 */

#pragma once

#define _PINNUM(port, pin) ((port) * 32 + (pin))

#include "WVariant.h"

////////////////////////////////////////////////////////////////////////////////
// Low frequency clock source

#define USE_LFXO    // 32.768 kHz crystal oscillator
#define VARIANT_MCK (64000000ul)

#define WIRE_INTERFACES_COUNT   (1)

////////////////////////////////////////////////////////////////////////////////
// Power

#define PIN_PWR_EN              (30) // RT9080_EN

#define BATTERY_PIN             (0 + 2)
#define ADC_MULTIPLIER          (4.90F)

#define ADC_RESOLUTION          (14)
#define BATTERY_SENSE_RES       (12)

#define AREF_VOLTAGE            (3.0)

////////////////////////////////////////////////////////////////////////////////
// Number of pins

#define PINS_COUNT              (48)
#define NUM_DIGITAL_PINS        (48)
#define NUM_ANALOG_INPUTS       (1)
#define NUM_ANALOG_OUTPUTS      (0)

////////////////////////////////////////////////////////////////////////////////
// UART pin definition

#define PIN_SERIAL1_RX          PIN_GPS_TX
#define PIN_SERIAL1_TX          PIN_GPS_RX
////////////////////////////////////////////////////////////////////////////////
// I2C pin definition

#define PIN_WIRE_SDA            (4) // (SDA)
#define PIN_WIRE_SCL            (2) // (SCL)

////////////////////////////////////////////////////////////////////////////////
// SPI pin definition

#define SPI_INTERFACES_COUNT    (2)

#define PIN_SPI_MISO            (17) // (MISO)
#define PIN_SPI_MOSI            (15) // (MOSI)
#define PIN_SPI_SCK             (13) // (SCK)
#define PIN_SPI_NSS             (-1)

////////////////////////////////////////////////////////////////////////////////
// QSPI FLASH

#define PIN_QSPI_SCK            (4)
#define PIN_QSPI_CS             (12)
#define PIN_QSPI_IO0            (6)
#define PIN_QSPI_IO1            (8)
#define PIN_QSPI_IO2            (32 + 9)
#define PIN_QSPI_IO3            (26)

#define EXTERNAL_FLASH_DEVICES ZD25WQ32CEIGR
#define EXTERNAL_FLASH_USE_QSPI

////////////////////////////////////////////////////////////////////////////////
// Builtin LEDs

#define LED_RED                 (32 + 14) // LED_3
#define LED_BLUE                (32 + 5)  // LED_2
#define LED_GREEN               (32 + 7)  // LED_1

//#define PIN_STATUS_LED          LED_BLUE
#define LED_BUILTIN             (-1)
#define LED_PIN                 LED_BUILTIN
#define LED_STATE_ON            LOW

////////////////////////////////////////////////////////////////////////////////
// Builtin buttons

#define PIN_BUTTON1             (24) // BOOT
#define BUTTON_PIN              PIN_BUTTON1
#define PIN_USER_BTN            BUTTON_PIN

#define PIN_BUTTON2             (18)
#define BUTTON_PIN2             PIN_BUTTON2

#define EXTERNAL_FLASH_DEVICES MX25R1635F
#define EXTERNAL_FLASH_USE_QSPI

////////////////////////////////////////////////////////////////////////////////
// Lora

#define USE_SX1262
#define LORA_CS                 (11) 
#define SX126X_POWER_EN         (30)
#define SX126X_DIO1             (32 + 8) 
#define SX126X_BUSY             (14)
#define SX126X_RESET            (7)
#define SX126X_RF_VC1           (27)
#define SX126X_RF_VC2           (33)

////////////////////////////////////////////////////////////////////////////////
// SPI1

#define PIN_SPI1_MISO           (-1)
#define PIN_SPI1_MOSI           (20)
#define PIN_SPI1_SCK            (19)

// GxEPD2 needs that for a panel that is not even used !
extern const int MISO;
extern const int MOSI;
extern const int SCK;

////////////////////////////////////////////////////////////////////////////////
// Display

// #define DISP_MISO               (-1)
#define DISP_MOSI               (20)
#define DISP_SCLK               (19)
#define DISP_CS                 (22)
#define DISP_DC                 (21)
#define DISP_RST                (28)
#define DISP_BUSY               (3)
#define DISP_POWER              (32 + 12)
// #define DISP_BACKLIGHT          (-1)

#define PIN_DISPLAY_CS          DISP_CS
#define PIN_DISPLAY_DC          DISP_DC
#define PIN_DISPLAY_RST         DISP_RST
#define PIN_DISPLAY_BUSY        DISP_BUSY

////////////////////////////////////////////////////////////////////////////////
// GPS

#define PIN_GPS_RX              (32 + 13) // RXD
#define PIN_GPS_TX              (32 + 15) // TXD
#define GPS_EN                  (32 + 11) // POWER_RT9080_EN
#define PIN_GPS_STANDBY         (32 + 10)
#define PIN_GPS_PPS             (29) // 1PPS
