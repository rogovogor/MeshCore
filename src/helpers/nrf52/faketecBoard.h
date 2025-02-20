#pragma once

#include <MeshCore.h>
#include <Arduino.h>

#define P_LORA_NSS D13 
#define P_LORA_DIO_1 D11
#define P_LORA_RESET D10
#define P_LORA_BUSY  D16
#define P_LORA_MISO  D15
#define P_LORA_SCLK  D12
#define P_LORA_MOSI  D14
#define SX126X_POWER_EN EXT_VCC

#define SX126X_DIO2_AS_RF_SWITCH  true
#define SX126X_DIO3_TCXO_VOLTAGE   1.8

#define  PIN_VBAT_READ PIN_A2
#define  ADC_MULTIPLIER   (1.815) // dependent on voltage divider resistors. TODO: more accurate battery tracking

class faketecBoard : public mesh::MainBoard {
protected:
  uint8_t startup_reason;

public:
  void begin() {
    // for future use, sub-classes SHOULD call this from their begin()
    startup_reason = BD_STARTUP_NORMAL;

    pinMode(PIN_VBAT_READ, INPUT);

    pinMode(SX126X_POWER_EN, OUTPUT);
    digitalWrite(SX126X_POWER_EN, HIGH);
    delay(10);   // give sx1262 some time to power up
  }

  uint8_t getStartupReason() const override { return startup_reason; }

  #define BATTERY_SAMPLES 8

  uint16_t getBattMilliVolts() override {
    analogReadResolution(12);

    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
      raw += analogRead(PIN_VBAT_READ);
    }
    raw = raw / BATTERY_SAMPLES;
    return (ADC_MULTIPLIER * raw);
  }

  const char* getManufacturerName() const override {
    return "Faketec DIY";
  }

  void reboot() override {
    NVIC_SystemReset();
  }

  bool startOTAUpdate() override;
};
