#pragma once
#include <../heltec_v3/HeltecV3Board.h>

class WirelessPaperBoard : public HeltecV3Board {
public:
  uint16_t getBattMilliVolts() override {
    digitalWrite(PIN_ADC_CTRL, PIN_ADC_CTRL_ACTIVE);

    uint32_t mv = 0;
    for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_VBAT_READ);
    mv /= 8;

    digitalWrite(PIN_ADC_CTRL, PIN_ADC_CTRL_INACTIVE);

    // Q3 (AO3401) + R21=10k/R26=10k voltage divider → multiplier = 2
    // Ref: https://resource.heltec.cn/download/Wireless_Paper/Wireless_Paper_V0.4_Schematic_Diagram.pdf
    return mv * 2;
  }

  const char* getManufacturerName() const override {
    return "Heltec Wireless Paper";
  }
};
