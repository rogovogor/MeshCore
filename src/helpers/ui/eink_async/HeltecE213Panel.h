#pragma once

#include <GxEPD2_BW.h>

// Панель Heltec Vision Master E213 — LCMEN2R13EFC1, контроллер семейства UC81xx
// (команды 0x00/0x10/0x12/0x13/0x20..0x24/0x50). Отдельный тип нужен, чтобы не
// трогать GxEPD2_213_B74: этот же класс используют платы WeAct, где асинхронный
// путь уже работает, и подмена таблиц сломала бы их.
class HeltecE213Panel : public GxEPD2_213_B74 {
public:
  HeltecE213Panel(int16_t cs, int16_t dc, int16_t rst, int16_t busy)
    : GxEPD2_213_B74(cs, dc, rst, busy) {}
};
