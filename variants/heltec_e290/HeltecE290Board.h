#pragma once

#include <Arduino.h>
#include <helpers/RefCountedDigitalPin.h>
#include <helpers/ESP32Board.h>

#ifndef ADC_MULTIPLIER
  #define ADC_MULTIPLIER 5.42
#endif

class HeltecE290Board : public ESP32Board {

public:
  RefCountedDigitalPin periph_power;

  HeltecE290Board() : periph_power(PIN_VEXT_EN, PIN_VEXT_EN_ACTIVE) { }

  float getAdcMultiplier() const override {
    return (adc_mult == 0.0f) ? ADC_MULTIPLIER : adc_mult;
  }
  void begin();
  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override ;

};
