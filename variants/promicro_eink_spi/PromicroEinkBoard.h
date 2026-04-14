#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/NRF52Board.h>

// LoRa SX1262 (E22/E22P) - SPI bus (D14/D15/D16)
#define P_LORA_NSS    13  // D13 = P1.13
#define P_LORA_DIO_1  11  // D11 = P0.10
#define P_LORA_RESET  10  // D10 = P0.09
#define P_LORA_BUSY   16  // D16 = P0.29
#define P_LORA_MISO   16  // D16 = P0.29 (shared with BUSY)
#define P_LORA_SCLK   14  // D14 = P1.15
#define P_LORA_MOSI   15  // D15 = P0.02

#define SX126X_POWER_EN       21  // D21 = P0.13
#define SX126X_RXEN           RADIOLIB_NC
#define SX126X_TXEN           RADIOLIB_NC
#define SX126X_DIO2_AS_RF_SWITCH  true
#define SX126X_DIO3_TCXO_VOLTAGE  (1.8f)

// E-ink display (SPI1 bus, D18/D19/D20)
#define PIN_DISPLAY_CS    5   // D5  = P0.24
#define PIN_DISPLAY_DC    12  // D12 = P1.11
#define PIN_DISPLAY_RST   7   // D7  = P0.11
#define PIN_DISPLAY_BUSY  8   // D8  = P1.04

// Encoder / UI
#define ENCODER_LEFT    3   // D3 = P0.20
#define ENCODER_RIGHT   4   // D4 = P0.22
#define ENCODER_PRESS   9   // D9 = P1.06
// BUTTON_PIN / BOOT as BACK = D6 = P1.00 (defined in variant.h as PIN_BUTTON1)

// GPS via Serial1 (D0=RX, D1=TX), enable on D2
#define PIN_GPS_EN  2   // D2 = P0.17

// Buzzer moved to D22 = P0.15 (LED_BUILTIN).
// This frees D17 = P0.31 (AIN7) for battery ADC.
// LED_BUILTIN will flash together with buzzer tones — useful visual feedback.
// PIN_BUZZER is defined in platformio.ini as 22.

// Battery ADC on D17 = P0.31 (AIN7).
// Requires external voltage divider: VBAT → 100kΩ → D17 → 100kΩ → GND.
// ADC_MULTIPLIER ≈ 1.73 for equal-value divider; tune with a known voltage.
#define PIN_VBAT_READ   17   // D17 = P0.31 = AIN7
#define ADC_MULTIPLIER  (1.73f)

class PromicroEinkBoard : public NRF52BoardDCDC {
protected:
  uint8_t btn_prev_state;
  float adc_mult = ADC_MULTIPLIER;

public:
  PromicroEinkBoard() : NRF52Board("ProMicroEink_OTA") {}
  void begin();

  #define BATTERY_SAMPLES 8

  uint16_t getBattMilliVolts() override {
    analogReadResolution(12);
    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
      raw += analogRead(PIN_VBAT_READ);
    }
    raw = raw / BATTERY_SAMPLES;
    return (uint16_t)(adc_mult * raw);
  }

  bool setAdcMultiplier(float multiplier) override {
    adc_mult = (multiplier == 0.0f) ? ADC_MULTIPLIER : multiplier;
    return true;
  }

  float getAdcMultiplier() const override {
    return (adc_mult == 0.0f) ? ADC_MULTIPLIER : adc_mult;
  }

  const char* getManufacturerName() const override {
    return "ProMicro WeAct Eink DIY";
  }

  int buttonStateChanged() {
    #ifdef BUTTON_PIN
      uint8_t v = digitalRead(BUTTON_PIN);
      if (v != btn_prev_state) {
        btn_prev_state = v;
        return (v == LOW) ? 1 : -1;
      }
    #endif
    return 0;
  }

  void powerOff() override {
    sd_power_system_off();
  }
};
