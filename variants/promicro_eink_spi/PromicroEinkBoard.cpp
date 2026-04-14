#include <Arduino.h>
#include "PromicroEinkBoard.h"

void PromicroEinkBoard::begin() {
    NRF52BoardDCDC::begin();
    btn_prev_state = HIGH;

    pinMode(PIN_VBAT_READ, INPUT);

    #ifdef BUTTON_PIN
      pinMode(BUTTON_PIN, INPUT_PULLUP);
    #endif

    // Radio power enable
    pinMode(SX126X_POWER_EN, OUTPUT);
    digitalWrite(SX126X_POWER_EN, HIGH);
    delay(10);   // give SX1262 time to power up

    // Wire / I2C intentionally not initialized (no I2C devices in this variant)
}
