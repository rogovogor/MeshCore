#pragma once
#include <../heltec_v3/HeltecV3Board.h>

class WirelessPaperBoard : public HeltecV3Board {
public:
  uint16_t getBattMilliVolts() override {
    analogReadResolution(10);
    digitalWrite(PIN_ADC_CTRL, adc_active_state);

    uint32_t raw = 0;
    for (int i = 0; i < 8; i++) raw += analogRead(PIN_VBAT_READ);
    raw /= 8;

    digitalWrite(PIN_ADC_CTRL, !adc_active_state);

    // Voltage divider R1=10k, R2=10k → multiplier = (R1+R2)/R2 = 2.0
    // Ref: https://resource.heltec.cn/download/Wireless_Paper/Wireless_Paper_V0.4_Schematic_Diagram.pdf
    return (2.0 * (3.3 / 1024.0) * raw) * 1000;
  }

  const char* getManufacturerName() const override {
    return "Heltec Wireless Paper";
  }
};
