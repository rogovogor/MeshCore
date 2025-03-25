#include <Arduino.h>
#include "target.h"

ESP32Board board;

ESPNOWRadio radio;

bool radio_init() {
  // NOTE: radio.begin() is called by Dispatcher::begin(), so not needed here
  return true;  // success
}
