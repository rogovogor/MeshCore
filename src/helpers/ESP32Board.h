#pragma once

#include <MeshCore.h>
#include <Arduino.h>

#ifndef USER_BTN_PRESSED
#define USER_BTN_PRESSED LOW
#endif

#if defined(ESP_PLATFORM)

#include <rom/rtc.h>
#include <sys/time.h>
#include <Wire.h>
#include "soc/rtc.h"
#include "esp_system.h"

class ESP32Board : public mesh::MainBoard {
protected:
  uint8_t startup_reason;
  bool inhibit_sleep = false;
  float adc_mult = 0.0f;  // 0.0f = use board default
  static inline portMUX_TYPE sleepMux = portMUX_INITIALIZER_UNLOCKED;

public:
  void begin() {
    // for future use, sub-classes SHOULD call this from their begin()
    startup_reason = BD_STARTUP_NORMAL;    

  #ifdef ESP32_CPU_FREQ
    setCpuFrequencyMhz(ESP32_CPU_FREQ);
  #endif

  #ifdef PIN_VBAT_READ
    // battery read support
    pinMode(PIN_VBAT_READ, INPUT);
    adcAttachPin(PIN_VBAT_READ);
  #endif

  #ifdef P_LORA_TX_LED
    pinMode(P_LORA_TX_LED, OUTPUT);
    digitalWrite(P_LORA_TX_LED, LOW);
  #endif

  #if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
   #if PIN_BOARD_SDA >= 0 && PIN_BOARD_SCL >= 0
    Wire.begin(PIN_BOARD_SDA, PIN_BOARD_SCL);
   #endif
  #else
    Wire.begin();
  #endif    
  }

  // Temperature from ESP32 MCU
  float getMCUTemperature() override {
    uint32_t raw = 0;

    // To get and average the temperature so it is more accurate, especially in low temperature
    for (int i = 0; i < 4; i++) {
      raw += temperatureRead();
    }

    return raw / 4;
  }

  uint32_t getIRQGpio() override {
    return P_LORA_DIO_1; // default for SX1262
  }

  void sleep(uint32_t secs) override {
    // Skip if not allow to sleep
    if (inhibit_sleep) {
      delay(1); // Give MCU to OTA to run
      return;
    }

    // Set GPIO wakeup
    gpio_num_t wakeupPin = (gpio_num_t)getIRQGpio();    

    // Configure timer wakeup
    if (secs > 0) {
      esp_sleep_enable_timer_wakeup(secs * 1000000ULL); // Wake up periodically to do scheduled jobs
    }

    // Disable CPU interrupt servicing
    portENTER_CRITICAL(&sleepMux);

    // Skip sleep if there is a LoRa packet
    if (gpio_get_level(wakeupPin) == HIGH) {
      portEXIT_CRITICAL(&sleepMux);
      delay(1);
      return;
    }

    // Configure GPIO wakeup
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable((gpio_num_t)wakeupPin, GPIO_INTR_HIGH_LEVEL); // Wake up when receiving a LoRa packet

    // MCU enters light sleep
    esp_light_sleep_start();

    // Avoid ISR flood during wakeup due to HIGH LEVEL interrupt
    gpio_wakeup_disable(wakeupPin);
    gpio_set_intr_type(wakeupPin, GPIO_INTR_POSEDGE);

    // Enable CPU interrupt servicing
    portEXIT_CRITICAL(&sleepMux);
  }

  uint8_t getStartupReason() const override { return startup_reason; }

#if defined(P_LORA_TX_LED)
  void onBeforeTransmit() override {
    digitalWrite(P_LORA_TX_LED, HIGH);   // turn TX LED on
  }
  void onAfterTransmit() override {
    digitalWrite(P_LORA_TX_LED, LOW);   // turn TX LED off
  }
#elif defined(P_LORA_TX_NEOPIXEL_LED)
  #define NEOPIXEL_BRIGHTNESS    64  // white brightness (max 255)

  void onBeforeTransmit() override {
    neopixelWrite(P_LORA_TX_NEOPIXEL_LED, NEOPIXEL_BRIGHTNESS, NEOPIXEL_BRIGHTNESS, NEOPIXEL_BRIGHTNESS);   // turn TX neopixel on (White)
  }
  void onAfterTransmit() override {
    neopixelWrite(P_LORA_TX_NEOPIXEL_LED, 0, 0, 0);   // turn TX neopixel off
  }
#endif

  bool setAdcMultiplier(float multiplier) override {
#ifdef PIN_VBAT_READ
    adc_mult = multiplier;
    return true;
#else
    return false;
#endif
  }

  float getAdcMultiplier() const override {
#ifdef PIN_VBAT_READ
    return (adc_mult == 0.0f) ? 2.0f : adc_mult;
#else
    return 0.0f;
#endif
  }

  uint16_t getBattMilliVolts() override {
    #ifdef PIN_VBAT_READ
    analogReadResolution(12);

    uint32_t raw = 0;
    for (int i = 0; i < 4; i++) {
      raw += analogReadMilliVolts(PIN_VBAT_READ);
    }
    raw = raw / 4;

    return (uint16_t)(getAdcMultiplier() * raw);
  #else
    return 0;  // not supported
  #endif
  }

  const char* getManufacturerName() const override {
    return "Generic ESP32";
  }

  void reboot() override {
    esp_restart();
  }

  bool startOTAUpdate(const char* id, char reply[]) override;

  void setInhibitSleep(bool inhibit) {
    inhibit_sleep = inhibit;
  }
};

class ESP32RTCClock : public mesh::RTCClock {
  bool time_was_set = false;
public:
  ESP32RTCClock() { }
  void begin() {
    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_POWERON) {
      // start with some date/time in the recent past
      struct timeval tv;
      tv.tv_sec = 1715770351;  // 15 May 2024, 8:50pm
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
  } else {
      // Anything other than a cold boot keeps the RTC domain alive, so the
      // time carried over from before the reset is as good as it was then.
      time_was_set = true;
  }
  }

  // The placeholder date above is inside any sane validity range, so callers
  // cannot tell it apart from a real time by value alone.
  bool isTimeReliable() const override { return time_was_set; }
  uint32_t getCurrentTime() override {
    time_t _now;
    time(&_now);
    return _now;
  }
  void setCurrentTime(uint32_t time) override {
    struct timeval tv;
    tv.tv_sec = time;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    time_was_set = true;
  }
};

#endif
