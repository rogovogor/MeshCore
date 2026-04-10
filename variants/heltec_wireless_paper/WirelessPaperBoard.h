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

    // R21=10k/R26=10k voltage divider, theoretical multiplier = 2,
    // corrected to 1.826 for ESP32-S3 ADC ~9.5% overcalibration at 11dB atten (~2V input).
    // Empirically derived: 3.9V actual -> 4.27V firmware -> factor = 2/1.095 = 1.826
    // Ref: https://resource.heltec.cn/download/Wireless_Paper/Wireless_Paper_V0.4_Schematic_Diagram.pdf
    return (mv * 1826) / 1000;
  }

  const char* getManufacturerName() const override {
    return "Heltec Wireless Paper";
  }
};
