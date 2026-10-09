#pragma once

#include <GxEPD2_BW.h>
#include "AsyncEinkCommon.h"

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
    if (partial && !_using_partial_mode) initPartialWaveform();
    if (!partial) _using_partial_mode = false;
    _writeCommand(0x22);
    _writeData(partial ? 0xcc : 0xf7);
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
    if (elapsed < ASYNC_EINK_BUSY_ASSERT_MS) return PollResult::Busy;
    if (panelBusy()) {
      if (elapsed <= _timeout_ms) return PollResult::Busy;
      completeOperation();
      return PollResult::TimedOut;
    }
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

  void initPartialWaveform() {
    static const uint8_t lut_partial[] PROGMEM = {
      0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
    };
    _writeCommand(0x32);
    _writeDataPGM(lut_partial, sizeof(lut_partial));
    _using_partial_mode = true;
  }
};
