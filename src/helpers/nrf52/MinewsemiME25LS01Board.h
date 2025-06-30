#pragma once

#include <MeshCore.h>
#include <Arduino.h>

// LoRa and SPI pins

#define  P_LORA_DIO_1   (32 + 12)  // P1.12
#define  P_LORA_NSS     (32 + 13)  // P1.13
#define  P_LORA_RESET   (32 + 11)  // P1.11
#define  P_LORA_BUSY    (32 + 10)  // P1.10
#define  P_LORA_SCLK    (32 + 15)  // P1.15
#define  P_LORA_MISO    (0 + 29)   // P0.29
#define  P_LORA_MOSI    (0 + 2)    // P0.2
 
#define LR11X0_DIO_AS_RF_SWITCH  true
#define LR11X0_DIO3_TCXO_VOLTAGE   1.6

// built-ins
//#define  PIN_VBAT_READ    5
//#define  ADC_MULTIPLIER   (3 * 1.73 * 1000)

class MinewsemiME25LS01Board : public mesh::MainBoard {
protected:
  uint8_t startup_reason;
  uint8_t btn_prev_state;

public:
  void begin();

  uint16_t getBattMilliVolts() override {
  #ifdef BATTERY_PIN
   #ifdef PIN_3V3_EN
    digitalWrite(PIN_3V3_EN, HIGH);
   #endif
    analogReference(AR_INTERNAL_3_0);
    analogReadResolution(12);
    delay(10);
    float volts = (analogRead(BATTERY_PIN) * ADC_MULTIPLIER * AREF_VOLTAGE) / 4096;
   #ifdef PIN_3V3_EN
    digitalWrite(PIN_3V3_EN, LOW);
   #endif

    analogReference(AR_DEFAULT);  // put back to default
    analogReadResolution(10);

    return volts * 1000;
  #else
    return 0;
  #endif
  }

  uint8_t getStartupReason() const override { return startup_reason; }

  const char* getManufacturerName() const override {
    return "m25ls01";
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
    #ifdef HAS_GPS
        digitalWrite(GPS_VRTC_EN, LOW);
        digitalWrite(GPS_RESET, LOW);
        digitalWrite(GPS_SLEEP_INT, LOW);
        digitalWrite(GPS_RTC_INT, LOW);
        pinMode(GPS_RESETB, OUTPUT);
        digitalWrite(GPS_RESETB, LOW);
    #endif
    
    #ifdef BUZZER_EN
        digitalWrite(BUZZER_EN, LOW);
    #endif
    
    #ifdef PIN_3V3_EN
        digitalWrite(PIN_3V3_EN, LOW);
    #endif

    #ifdef LED_PIN
    digitalWrite(LED_PIN, LOW);
    #endif
    #ifdef BUTTON_PIN
    nrf_gpio_cfg_sense_input(digitalPinToInterrupt(BUTTON_PIN), NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_HIGH);
    #endif
    sd_power_system_off();
  }

  #if defined(P_LORA_TX_LED)
  void onBeforeTransmit() override {
    digitalWrite(P_LORA_TX_LED, HIGH);// turn TX LED on
  }
  void onAfterTransmit() override {
    digitalWrite(P_LORA_TX_LED, LOW); // turn TX LED off
  }
#endif


  void reboot() override {
    NVIC_SystemReset();
  }

//  bool startOTAUpdate(const char* id, char reply[]) override;
};