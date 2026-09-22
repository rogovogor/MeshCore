#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>

// Battery voltage-divider ratio for this board.
#ifndef BATTERY_ADC_MULTIPLIER
  #define BATTERY_ADC_MULTIPLIER 2.0f
#endif

class BuildLabFrostNodeBoard : public ESP32Board {
public:
  void begin();
  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override { return "BuildLab Frost Node"; }
};
