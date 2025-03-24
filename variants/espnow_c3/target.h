#pragma once

#include <helpers/ESP32Board.h>
#include <helpers/esp32/ESPNOWRadio.h>

extern ESP32Board board;
extern ESPNOWRadio radio;

bool radio_init();
