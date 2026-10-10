#pragma once

#include <GxEPD2_BW.h>
#include "AsyncEinkCommon.h"
#include "Depg0290PartialLut.h"

// Non-blocking extension for GxEPD2 1.6.4's GxEPD2_290_BS driver.
// This panel has no partial waveform in OTP, so the small LUT below is retained
// verbatim from the upstream driver (GxEPD2, GPL-3.0).
class AsyncGxEPD290BS : public GxEPD2_290_BS {
public:
  using PollResult = AsyncEinkPollResult;

  AsyncGxEPD290BS(int16_t cs, int16_t dc, int16_t rst, int16_t busy)
    : GxEPD2_290_BS(cs, dc, rst, busy) {}

  bool startRefreshAsync(bool partial) {
    if (_operation != Operation::None) return false;
#if defined(EINK_ASYNC_LOG)
    Serial.printf("[EINK] refresh start partial=%d busy_pin=%d\n", partial ? 1 : 0, digitalRead(PIN_DISPLAY_BUSY));
#endif
    if (partial && !_using_partial_mode) initPartialWaveform();
    if (!partial) _using_partial_mode = false;
    _writeCommand(0x22);
    _writeData(partial ? 0xcf : 0xf7);   // 0xCF — как в рабочем драйвере панели, а не 0xCC
    _writeCommand(0x20);
    start(Operation::Refresh, partial ? partial_refresh_time : full_refresh_time, partial);
    return true;
  }

  bool startPowerOffAsync() {
    if (_operation != Operation::None) return false;
    if (!_power_is_on) {
      _using_partial_mode = false;
      return false;
    }
    _writeCommand(0x22);
    _writeData(0x83);
    _writeCommand(0x20);
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
    // Номинал — у запущенной операции: выключение питания панели держится свои
    // ~150–250 мс, а не время полного обновления (иначе после каждого кадра
    // экран 4 с «занят» и кнопки отвечают с той же задержкой).
    const uint32_t nominal = _nominal_ms;
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
  uint32_t _nominal_ms = 0;
  bool _partial_refresh = false;

  void start(Operation operation, uint32_t nominal_ms, bool partial) {
    _operation = operation;
    _started_at = millis();
    _nominal_ms = nominal_ms;
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

  void initPartialWaveform() {
    // Панель DEPG0290BNS800 (Vision Master E290): своя последовательность
    // частичного обновления и своя таблица волновой формы. Перенесено из
    // рабочего драйвера heltec-eink-modules (DEPG0290BNS800/mode.cpp), который
    // на этой плате действительно рисует, — вместо чужой таблицы GxEPD2.
    _writeCommand(0x3C); _writeData(0x60);                                       // форма границы
    _writeCommand(0x04); _writeData(0x41); _writeData(0x00); _writeData(0x32);   // напряжения источников, ±15 В
    _writeCommand(0x32);                                                        // своя таблица
    for (size_t i = 0; i < sizeof(DEPG0290_LUT_PARTIAL); i++) _writeData(DEPG0290_LUT_PARTIAL[i]);
    _writeCommand(0x37);                                                        // режим ping-pong
    for (int i = 0; i < 5; i++) _writeData(0x00);
    _writeData(0x40);
    for (int i = 0; i < 4; i++) _writeData(0x00);
    _using_partial_mode = true;
  }
};
