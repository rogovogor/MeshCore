#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/NRF52Board.h>

// LoRa radio module pins for RAK3401 with RAK13302 (uses SPI1)
#define  P_LORA_DIO_1   10
#define  P_LORA_NSS     26
#define  P_LORA_RESET   4
#define  P_LORA_BUSY    9
#define  P_LORA_SCLK    3   // SPI1_SCK
#define  P_LORA_MISO    29  // SPI1_MISO
#define  P_LORA_MOSI    30  // SPI1_MOSI
#define  SX126X_POWER_EN  21

//#define PIN_GPS_SDA       13  //GPS SDA pin (output option)
//#define PIN_GPS_SCL       14  //GPS SCL pin (output option)
//#define PIN_GPS_TX        16  //GPS TX pin
//#define PIN_GPS_RX        15  //GPS RX pin
#define PIN_GPS_1PPS      17  //GPS PPS pin
#define GPS_BAUD_RATE   9600
#define GPS_ADDRESS   0x42  //i2c address for GPS

// RAK13302 1W LoRa transceiver module PA control (WisBlock IO slot)
// The RAK13302 mounts to the IO slot and has an ANT_SW (antenna switch) pin that controls the PA
// This pin must be controlled during transmission to enable the 1W power amplifier
//
// According to RAK13302 datasheet: ANT_SW connects to IO3 on the IO slot
// RAK19007 base board pin mapping: IO3 = pin 31 (also available as AIN1/A1 for analog input)
//
// Default: Pin 31 (IO3) - ANT_SW pin from RAK13302 datasheet
// Override by defining P_LORA_PA_EN in platformio.ini if needed
#ifndef P_LORA_PA_EN
  #define P_LORA_PA_EN  31  // ANT_SW pin from RAK13302 datasheet (IO3, pin 31 on RAK19007)
#endif

#define SX126X_DIO2_AS_RF_SWITCH  true
#define SX126X_DIO3_TCXO_VOLTAGE   1.8

// built-ins
#define  PIN_VBAT_READ    5
#define  ADC_MULTIPLIER   (3 * 1.73 * 1.187 * 1000)

// 3.3V periphery enable (GPS, IO Module, etc.)
#define PIN_3V3_EN (34)
#define WB_IO2 PIN_3V3_EN

class RAK3401Board : public NRF52BoardDCDC, public NRF52BoardOTA {
public:
  RAK3401Board() : NRF52BoardOTA("RAK3401_OTA") {}
  void begin();

  #define BATTERY_SAMPLES 8

  uint16_t getBattMilliVolts() override {
    analogReadResolution(12);

    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
      raw += analogRead(PIN_VBAT_READ);
    }
    raw = raw / BATTERY_SAMPLES;

    return (ADC_MULTIPLIER * raw) / 4096;
  }

  const char* getManufacturerName() const override {
    return "RAK 3401";
  }

#ifdef P_LORA_PA_EN
  void onBeforeTransmit() override {
    digitalWrite(P_LORA_PA_EN, HIGH);  // Enable PA before transmission
  }

  void onAfterTransmit() override {
    digitalWrite(P_LORA_PA_EN, LOW);   // Disable PA after transmission to save power
  }
#endif
};
