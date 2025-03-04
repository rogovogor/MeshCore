#pragma once

#include <Arduino.h>
#include "ESP32Board.h"
#include <driver/rtc_io.h>

// LoRa radio module pins for LilyGo T3S3
// These need to be defined here for the SX1276 version
#if defined(RADIO_CLASS) && defined(CustomSX1276)
  // SX1276 pin definitions specific to LilyGo T3S3
  #define P_LORA_DIO_0   34    // DIO0 interrupt pin (SX1276 uses DIO0)
  #define P_LORA_DIO_1   33    // DIO1 interrupt pin
  #define P_LORA_NSS     7     // Chip select
  #define P_LORA_RESET   8     // Reset pin
  #define P_LORA_SCLK    5     // SPI clock
  #define P_LORA_MISO    3     // SPI MISO
  #define P_LORA_MOSI    6     // SPI MOSI
#endif

// Base class for LilyGo T-series boards
class LilyGoTBoard : public ESP32Board {
public:
  void begin() {
    ESP32Board::begin();

    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_DEEPSLEEP) {
      long wakeup_source = esp_sleep_get_ext1_wakeup_status();
      if (wakeup_source & (1 << P_LORA_DIO_1)) {  // received a LoRa packet (while in deep sleep)
        startup_reason = BD_STARTUP_RX_PACKET;
      }

      rtc_gpio_hold_dis((gpio_num_t)P_LORA_NSS);
      rtc_gpio_deinit((gpio_num_t)P_LORA_DIO_1);
    }
  }

  void enterDeepSleep(uint32_t secs, int pin_wake_btn = -1) {
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    // Make sure the DIO1 and NSS GPIOs are hold on required levels during deep sleep 
    rtc_gpio_set_direction((gpio_num_t)P_LORA_DIO_1, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_en((gpio_num_t)P_LORA_DIO_1);

    rtc_gpio_hold_en((gpio_num_t)P_LORA_NSS);

    if (pin_wake_btn < 0) {
      esp_sleep_enable_ext1_wakeup( (1L << P_LORA_DIO_1), ESP_EXT1_WAKEUP_ANY_HIGH);  // wake up on: recv LoRa packet
    } else {
      esp_sleep_enable_ext1_wakeup( (1L << P_LORA_DIO_1) | (1L << pin_wake_btn), ESP_EXT1_WAKEUP_ANY_HIGH);  // wake up on: recv LoRa packet OR wake btn
    }

    if (secs > 0) {
      esp_sleep_enable_timer_wakeup(secs * 1000000);
    }

    // Finally set ESP32 into sleep
    esp_deep_sleep_start();   // CPU halts here and never returns!
  }

  uint16_t getBattMilliVolts() override {
    analogReadResolution(12);

    uint32_t raw = 0;
    for (int i = 0; i < 8; i++) {
      raw += analogReadMilliVolts(PIN_VBAT_READ);
    }
    raw = raw / 8;

    return (2 * raw);
  }
};

// Standard ESP32 version (for T3 board)
class LilyGoT3Board : public LilyGoTBoard {
public:
  const char* getManufacturerName() const override {
    return "LilyGo T3";
  }
};

// For S3 variant
class LilyGoT3S3Board : public LilyGoTBoard {
public:
  const char* getManufacturerName() const override {
    return "LilyGo T3S3";
  }
};