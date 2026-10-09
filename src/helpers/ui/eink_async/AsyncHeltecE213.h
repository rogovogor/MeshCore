#pragma once

#include <GxEPD2_BW.h>
#include "AsyncEinkCommon.h"
#include "HeltecE213Panel.h"
#include "HeltecE213Luts.h"

// Non-blocking extension for GxEPD2 1.6.4's HeltecE213Panel driver.
// The command bytes mirror that driver's _Update_Full(), _Update_Part() and
// _PowerOff(). Pixel transfer and controller initialization remain in GxEPD2.
class AsyncHeltecE213 : public HeltecE213Panel {
public:
  using PollResult = AsyncEinkPollResult;

  AsyncHeltecE213(int16_t cs, int16_t dc, int16_t rst, int16_t busy)
    : HeltecE213Panel(cs, dc, rst, busy) {}

  bool startRefreshAsync(bool partial) {
    if (_operation != Operation::None) return false;
#if defined(EINK_ASYNC_LOG)
    Serial.printf("[EINK] refresh start partial=%d busy_pin=%d\n", partial ? 1 : 0, digitalRead(PIN_DISPLAY_BUSY));
#endif
    // Панель LCMEN2R13EFC1 — контроллер UC81xx: свои команды запуска
    // (0x04 питание, 0x12 обновление), а не 0x22/0x20 как у SSD1680.
    if (partial && !_using_partial_mode) initPartialWaveform();
    if (!partial) _using_partial_mode = false;
    _writeCommand(0x04);
    _writeCommand(0x12);
    start(Operation::Refresh, partial ? partial_refresh_time : full_refresh_time, partial);
    return true;
  }

  void initPartialWaveform() {
    // Панель LCMEN2R13EFC1: последовательность частичного обновления и пять
    // таблиц. Перенесено из рабочего драйвера heltec-eink-modules
    // (LCMEN2R13EFC1/mode.cpp), который на этой плате действительно рисует.
    _writeCommand(0x00); _writeData(0xFF);   // panel setting
    _writeCommand(0x50); _writeData(0xD7);   // VCOM и интервал данных
    _writeCommand(0x20); for (size_t i = 0; i < sizeof(LUT_PARTIAL_VCOM_DC); i++) _writeData(LUT_PARTIAL_VCOM_DC[i]);
    _writeCommand(0x21); for (size_t i = 0; i < sizeof(LUT_PARTIAL_WW); i++) _writeData(LUT_PARTIAL_WW[i]);
    _writeCommand(0x22); for (size_t i = 0; i < sizeof(LUT_PARTIAL_BW); i++) _writeData(LUT_PARTIAL_BW[i]);
    _writeCommand(0x23); for (size_t i = 0; i < sizeof(LUT_PARTIAL_WB); i++) _writeData(LUT_PARTIAL_WB[i]);
    _writeCommand(0x24); for (size_t i = 0; i < sizeof(LUT_PARTIAL_BB); i++) _writeData(LUT_PARTIAL_BB[i]);
    _using_partial_mode = true;
  }

  bool startPowerOffAsync() {
    if (_operation != Operation::None) return false;
    if (!_power_is_on) {
      _using_partial_mode = false;
      return false;
    }
    _writeCommand(0x02);   // UC81xx: выключение питания панели
    start(Operation::PowerOff, power_off_time, false);
    return true;
  }

  bool asyncBusy() const { return _operation != Operation::None; }

  PollResult pollAsync() {
    if (_operation == Operation::None) return PollResult::Idle;
    const uint32_t elapsed = millis() - _started_at;
    // Панель обязана отработать волновую форму целиком: не завершаем операцию
    // раньше номинального времени обновления. Иначе выходит "done after 10 ms"
    // при реальных ~600 мс — кадр не прорисовывается, и экран остаётся на
    // первой надписи. Линия занятости остаётся признаком, но лишь уточняющим.
    const uint32_t nominal = _partial_refresh ? partial_refresh_time : full_refresh_time;
    if (elapsed < nominal) {
      if (elapsed > _timeout_ms) { completeOperation(); return PollResult::TimedOut; }
      return PollResult::Busy;
    }
    if (elapsed < ASYNC_EINK_BUSY_ASSERT_MS) return PollResult::Busy;
    if (panelBusy()) {
      if (elapsed <= _timeout_ms) return PollResult::Busy;
#if defined(EINK_ASYNC_LOG)
      Serial.printf("[EINK] refresh TIMEOUT after %lu ms (busy_pin=%d)\n", (unsigned long)elapsed, digitalRead(PIN_DISPLAY_BUSY));
#endif
      completeOperation();
      return PollResult::TimedOut;
    }
#if defined(EINK_ASYNC_LOG)
    Serial.printf("[EINK] refresh done after %lu ms (busy_pin=%d)\n", (unsigned long)elapsed, digitalRead(PIN_DISPLAY_BUSY));
#endif
    completeOperation();
    return PollResult::Complete;
  }

private:
  enum class Operation : uint8_t { None, Refresh, PowerOff };
  Operation _operation = Operation::None;
  uint32_t _started_at = 0;
  uint32_t _timeout_ms = 0;
  bool _partial_refresh = false;

  void start(Operation operation, uint32_t nominal_ms, bool partial) {
    _operation = operation;
    _started_at = millis();
    _timeout_ms = nominal_ms + ASYNC_EINK_TIMEOUT_SLACK_MS;
    _partial_refresh = partial;
  }

  bool panelBusy() const {
    if (_busy < 0) return (millis() - _started_at) < (_timeout_ms - ASYNC_EINK_TIMEOUT_SLACK_MS);
    return digitalRead(_busy) == _busy_level;
  }

  void completeOperation() {
    if (_operation == Operation::Refresh) {
      _initial_refresh = false;
      _power_is_on = _partial_refresh;
    } else if (_operation == Operation::PowerOff) {
      _power_is_on = false;
      _using_partial_mode = false;
    }
    _operation = Operation::None;
  }
};
