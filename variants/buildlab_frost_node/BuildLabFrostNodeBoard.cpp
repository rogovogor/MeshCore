#include "BuildLabFrostNodeBoard.h"

void BuildLabFrostNodeBoard::begin() {
  ESP32Board::begin();

#ifdef PIN_GPS_EN
  // MeshCore's UART-GPS init never drives PIN_GPS_EN; power the module here
  // (active-high on this board).
  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, HIGH);
#endif
}

uint16_t BuildLabFrostNodeBoard::getBattMilliVolts() {
#ifdef PIN_VBAT_READ
  analogReadResolution(12);

  uint32_t raw = 0;
  for (int i = 0; i < 8; i++) {
    raw += analogReadMilliVolts(PIN_VBAT_READ);
  }
  raw = raw / 8;

  return (uint16_t)(raw * BATTERY_ADC_MULTIPLIER);
#else
  return 0;  // not supported
#endif
}
