#pragma once

#include <Arduino.h>

class RefCountedDigitalPin {
  uint8_t _pin;
  int8_t _claims = 0;
  uint8_t _active = 0;
public:
  RefCountedDigitalPin(uint8_t pin,uint8_t active=HIGH): _pin(pin), _active(active) { }

  void begin() {
    pinMode(_pin, OUTPUT);
    // Захват мог случиться до begin() (статический конструктор в target.cpp
    // асинхронных e-ink сборок): тогда пин должен остаться включённым, иначе
    // счётчик говорит «включено», а рельс выключен и больше не включится.
    digitalWrite(_pin, _claims > 0 ? _active : !_active);
  }

  void claim() {
    _claims++;
    if (_claims > 0) {
      digitalWrite(_pin, _active);
    }
  }

  void release() {
    if (_claims == 0) return; // avoid negative _claims

    _claims--;
    if (_claims == 0) {
      digitalWrite(_pin, !_active);
    }
  }
};
