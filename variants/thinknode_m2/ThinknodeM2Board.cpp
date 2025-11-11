#include "ThinknodeM2Board.h"



void ThinknodeM2Board::begin() {
    ESP32Board::begin();
    pinMode(PIN_VEXT_EN, OUTPUT); // init display
    digitalWrite(PIN_VEXT_EN, PIN_VEXT_EN_ACTIVE); // pin needs to be high
    delay(10); 
    digitalWrite(PIN_VEXT_EN, PIN_VEXT_EN_ACTIVE); // need to do this twice. do not know why..
    pinMode(PIN_STATUS_LED, OUTPUT); // init power led    
  }

  void ThinknodeM2Board::enterDeepSleep(uint32_t secs, int pin_wake_btn) {
    esp_deep_sleep_start();
  }

  void ThinknodeM2Board::powerOff()  {
    enterDeepSleep(0);
  }

  uint16_t ThinknodeM2Board::getBattMilliVolts()  {
    analogReadResolution(12);
    delay(10);
    float volts = (analogRead(PIN_VBAT_READ) * ADC_MULTIPLIER * AREF_VOLTAGE) / 4096;
    analogReadResolution(10);
    return volts * 1000;
  }

  const char* ThinknodeM2Board::getManufacturerName() const {
    return "Elecrow ThinkNode M2";
  }