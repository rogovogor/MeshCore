#include <Arduino.h>
#include "target.h"

ESP32Board board;

ESPNOWRadio radio;

bool radio_init() {
  radio.begin();
  return true;  // success
}
