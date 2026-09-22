#include "MomentaryButton.h"

#define MULTI_CLICK_WINDOW_MS  280

// ── Static ISR dispatch ──────────────────────────────────────────────────────
// File-local: up to 8 buttons can register GPIO interrupts.

static MomentaryButton* s_irq_instances[8] = {};
static uint8_t s_irq_count = 0;

static void s_dispatch(uint8_t idx) {
  if (s_irq_instances[idx]) s_irq_instances[idx]->_notifyPressed();
}
static void s_isr0() { s_dispatch(0); }
static void s_isr1() { s_dispatch(1); }
static void s_isr2() { s_dispatch(2); }
static void s_isr3() { s_dispatch(3); }
static void s_isr4() { s_dispatch(4); }
static void s_isr5() { s_dispatch(5); }
static void s_isr6() { s_dispatch(6); }
static void s_isr7() { s_dispatch(7); }

typedef void (*ISRFunc)();
static const ISRFunc s_isr_table[8] = {
  s_isr0, s_isr1, s_isr2, s_isr3, s_isr4, s_isr5, s_isr6, s_isr7
};

// ── Constructor / begin ──────────────────────────────────────────────────────

MomentaryButton::MomentaryButton(int8_t pin, int long_press_millis, bool reverse, bool pulldownup, bool multiclick) {
  _pin = pin;
  _reverse = reverse;
  _pull = pulldownup;
  down_at = 0;
  prev = _reverse ? HIGH : LOW;
  cancel = 0;
  _long_millis = long_press_millis;
  _threshold = 0;
  _click_count = 0;
  _last_click_time = 0;
  _multi_click_window = multiclick ? MULTI_CLICK_WINDOW_MS : 0;
  _pending_click = false;
}

MomentaryButton::MomentaryButton(int8_t pin, int long_press_millis, int analog_threshold, bool multiclick) {
  _pin = pin;
  _reverse = false;
  _pull = false;
  down_at = 0;
  prev = LOW;
  cancel = 0;
  _long_millis = long_press_millis;
  _threshold = analog_threshold;
  _click_count = 0;
  _last_click_time = 0;
  _multi_click_window = multiclick ? MULTI_CLICK_WINDOW_MS : 0;
  _pending_click = false;
}

void MomentaryButton::begin() {
  if (_pin >= 0 && _threshold == 0) {
    pinMode(_pin, _pull ? (_reverse ? INPUT_PULLUP : INPUT_PULLDOWN) : INPUT);
  }
}

void MomentaryButton::enableInterrupt() {
  if (_pin < 0 || _threshold != 0) return;  // digital pins only
  if (s_irq_count >= 8) return;
  uint8_t idx = s_irq_count++;
  s_irq_instances[idx] = this;
  // For reverse=true (pullup) buttons: press = pin LOW → FALLING edge.
  // For normal (pulldown) buttons: press = pin HIGH → RISING edge.
  // Mode passed as a literal: its type differs between cores (int on nRF52/ESP32,
  // PinStatus on RP2040), so it must not go through an int variable.
  if (_reverse) attachInterrupt(digitalPinToInterrupt(_pin), s_isr_table[idx], FALLING);
  else          attachInterrupt(digitalPinToInterrupt(_pin), s_isr_table[idx], RISING);
}

// ── isPressed ────────────────────────────────────────────────────────────────

bool MomentaryButton::isPressed() const {
  int btn = _threshold > 0 ? (analogRead(_pin) < _threshold) : digitalRead(_pin);
  return isPressed(btn);
}

void MomentaryButton::cancelClick() {
  cancel = 1;
  down_at = 0;
  _click_count = 0;
  _last_click_time = 0;
  _pending_click = false;
}

bool MomentaryButton::isPressed(int level) const {
  if (_threshold > 0) {
    return level;
  }
  if (_reverse) {
    return level == LOW;
  } else {
    return level != LOW;
  }
}

// ── check() ──────────────────────────────────────────────────────────────────

int MomentaryButton::check(bool repeat_click) {
  if (_pin < 0) return BUTTON_EVENT_NONE;

  int event = BUTTON_EVENT_NONE;
  int btn = _threshold > 0 ? (analogRead(_pin) < _threshold) : digitalRead(_pin);

  // ISR captured a press edge that occurred during a blocking call (e.g. eInk refresh).
  // If the button is currently UP it means the full press+release was missed by the poll.
  // Synthesize a pending click so it fires immediately on this check() call.
  if (_irq_pressed) {
    if (!isPressed(btn) && down_at == 0 && !cancel) {
      _click_count++;
      _last_click_time = millis();
      _pending_click = true;
    }
    _irq_pressed = false;
  }

  if (btn != prev) {
    if (isPressed(btn)) {
      down_at = millis();
      _irq_pressed = false;  // normal DOWN detected; IRQ flag is redundant
    } else {
      // button UP
      if (_long_millis > 0) {
        if (down_at > 0 && (unsigned long)(millis() - down_at) < _long_millis) {
            _click_count++;
            _last_click_time = millis();
            _pending_click = true;
        }
      } else {
          _click_count++;
          _last_click_time = millis();
          _pending_click = true;
      }
      if (event == BUTTON_EVENT_CLICK && cancel) {
        event = BUTTON_EVENT_NONE;
        _click_count = 0;
        _last_click_time = 0;
        _pending_click = false;
      }
      down_at = 0;
    }
    prev = btn;
  }
  if (!isPressed(btn) && cancel) {
    cancel = 0;
  }

  if (_long_millis > 0 && down_at > 0 && (unsigned long)(millis() - down_at) >= _long_millis) {
    if (_pending_click) {
      cancelClick();
    } else {
      event = BUTTON_EVENT_LONG_PRESS;
      down_at = 0;
      _click_count = 0;
      _last_click_time = 0;
      _pending_click = false;
    }
  }
  if (down_at > 0 && repeat_click) {
    unsigned long diff = (unsigned long)(millis() - down_at);
    if (diff >= 700) {
      event = BUTTON_EVENT_CLICK;
    }
  }

  if (_pending_click && (millis() - _last_click_time) >= (unsigned long)_multi_click_window) {
    if (down_at > 0) {
      return event;
    }
    switch (_click_count) {
      case 1:  event = BUTTON_EVENT_CLICK;        break;
      case 2:  event = BUTTON_EVENT_DOUBLE_CLICK; break;
      case 3:  event = BUTTON_EVENT_TRIPLE_CLICK; break;
      default: event = BUTTON_EVENT_TRIPLE_CLICK; break;
    }
    _click_count = 0;
    _last_click_time = 0;
    _pending_click = false;
  }

  return event;
}
