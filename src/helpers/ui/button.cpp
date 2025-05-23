#include "Button.h"

Button::Button() :
    _pin(0),
    _inputMode(INPUT_PULLUP),
    _buttonPressedState(HIGH), // Assuming INPUT_PULLUP, so HIGH is unpressed
    _lastButtonState(HIGH),
    _buttonState(false),      // Debounced state: false is unpressed
    _lastDebouncedState(false),
    _lastDebounceTime(0),
    _pressStartTime(0),
    _releaseTime(0),
    _currentSequenceCount(0),
    _lastPressEndTime(0),
    _callbackCount(0)
{
    for (uint8_t i = 0; i < MAX_PRESSES_IN_SEQUENCE; ++i) {
        _currentSequence[i] = ButtonPressType::NONE;
    }
    for (uint8_t i = 0; i < MAX_CALLBACKS; ++i) {
        _registeredCallbacks[i].count = 0;
        _registeredCallbacks[i].callback = nullptr;
        _registeredCallbacks[i].triggeredInSequence = false;
        for (uint8_t j = 0; j < MAX_PRESSES_IN_SEQUENCE; ++j) {
            _registeredCallbacks[i].sequence[j] = ButtonPressType::NONE;
        }
    }
}

void Button::attach(uint8_t pin, uint8_t inputMode) {
    _pin = pin;
    _inputMode = inputMode;
    pinMode(_pin, _inputMode);
    if (_inputMode == INPUT_PULLUP) {
        _buttonPressedState = LOW; // Pressed is LOW for PULLUP
        _lastButtonState = HIGH;   // Initial unpressed state
    } else {
        _buttonPressedState = HIGH; // Pressed is HIGH for PULLDOWN or regular INPUT
        _lastButtonState = LOW;    // Initial unpressed state
    }
    _lastDebouncedState = false; // Not pressed
    _buttonState = false;        // Not pressed
}

void Button::update() {
    if (_pin == 0) return; // Not attached

    bool reading = digitalRead(_pin);
    unsigned long currentTime = millis();

    // Debounce logic
    if (reading != _lastButtonState) {
        _lastDebounceTime = currentTime;
    }
    _lastButtonState = reading;

    if ((currentTime - _lastDebounceTime) > DEBOUNCE_DELAY) {
        bool currentDebouncedState = (reading == _buttonPressedState);

        // Edge detection
        if (currentDebouncedState != _lastDebouncedState) {
            _lastDebouncedState = currentDebouncedState;

            if (currentDebouncedState) { // Button pressed
                _pressStartTime = currentTime;
                // If a new press starts too long after the previous one, reset sequence
                if (_currentSequenceCount > 0 && (currentTime - _lastPressEndTime) > SHORT_PRESS_TIMEOUT) {
                    _resetSequence();
                }
            } else { // Button released
                _releaseTime = currentTime;
                unsigned long pressDuration = _releaseTime - _pressStartTime;
                ButtonPressType pressType = _determinePressType(pressDuration);

                if (pressType != ButtonPressType::NONE) {
                    _addPressToSequence(pressType);
                    _lastPressEndTime = currentTime; // Mark end time for next press timeout
                    _evaluateSequences();
                }
            }
        }
    }

    // Check for sequence timeout if a sequence is in progress
    if (_currentSequenceCount > 0 && (currentTime - _lastPressEndTime) > SHORT_PRESS_TIMEOUT) {
        // If we timed out, and haven't triggered anything specific from the partial sequence,
        // it's time to reset. Callbacks for partial matches should have already fired in _evaluateSequences.
        // This handles the case where a sequence like S1 is defined, and the user presses S, then waits.
        // We need to ensure S1 callback fires if it exists, even if S2 or S3 are also defined.
        // _evaluateSequences called on release handles this. This timeout mainly resets for the next input.
        _resetSequence();
    }
}


ButtonPressType Button::_determinePressType(unsigned long pressDuration) {
    if (pressDuration >= LONG_LONG_PRESS_MIN_DURATION) {
        return ButtonPressType::LL;
    } else if (pressDuration >= LONGISH_PRESS_MIN_DURATION && pressDuration < LONGISH_PRESS_MAX_DURATION) {
        return ButtonPressType::LS;
    } else if (pressDuration > DEBOUNCE_DELAY && pressDuration <= SHORT_PRESS_MAX_DURATION) { // Ensure it's more than debounce
        return ButtonPressType::S;
    }
    return ButtonPressType::NONE;
}

void Button::_addPressToSequence(ButtonPressType pressType) {
    if (_currentSequenceCount < MAX_PRESSES_IN_SEQUENCE) {
        // Specific handling for LL: it can only be LL1
        if (pressType == ButtonPressType::LL) {
            if (_currentSequenceCount == 0) { // LL only valid as the first press in its "sequence"
                 _currentSequence[_currentSequenceCount++] = pressType;
            } else {
                // An LL press occurred after other presses, which is not a defined LL1 sequence.
                // Treat as an invalid sequence progression, so reset.
                _resetSequence();
                // Potentially start a new sequence with this LL if it was intended as LL1.
                // For simplicity now, we just reset. More complex logic could try to salvage.
            }
        } else if (pressType == ButtonPressType::LS) {
            // LS can form sequences up to LS3
            // Check if previous was also LS or if sequence is empty
            if (_currentSequenceCount == 0 || _currentSequence[_currentSequenceCount -1] == ButtonPressType::LS) {
                _currentSequence[_currentSequenceCount++] = pressType;
            } else {
                // Mixing LS with S in a sequence is not explicitly defined as LS1/2/3 or S1/2/3
                // So, reset the sequence and start new with this LS.
                _resetSequence();
                _currentSequence[_currentSequenceCount++] = pressType;
            }
        } else if (pressType == ButtonPressType::S) {
            // S can form sequences up to S3
            // Check if previous was also S or if sequence is empty
             if (_currentSequenceCount == 0 || _currentSequence[_currentSequenceCount -1] == ButtonPressType::S) {
                _currentSequence[_currentSequenceCount++] = pressType;
            } else {
                // Mixing S with LS in a sequence is not S1/2/3 or LS1/2/3
                // So, reset the sequence and start new with this S.
                _resetSequence();
                _currentSequence[_currentSequenceCount++] = pressType;
            }
        }
    } else {
        // Sequence array is full, this shouldn't normally happen if MAX_PRESSES_IN_SEQUENCE is large enough.
        // Reset to be safe.
        _resetSequence();
        // And add the current press as the start of a new sequence.
        if (pressType != ButtonPressType::NONE) { // ensure it's a valid press
             _currentSequence[_currentSequenceCount++] = pressType;
        }
    }
}

void Button::_evaluateSequences() {
    if (_currentSequenceCount == 0) return;

    bool exactMatchFound = false;

    for (uint8_t i = 0; i < _callbackCount; ++i) {
        ButtonSequence& registered = _registeredCallbacks[i];
        if (registered.callback == nullptr) continue;

        if (registered.count == _currentSequenceCount) {
            bool match = true;
            for (uint8_t j = 0; j < _currentSequenceCount; ++j) {
                if (_currentSequence[j] != registered.sequence[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                // Exact match found
                if (!registered.triggeredInSequence) { // Check if already triggered for this evaluation pass
                    registered.callback();
                    registered.triggeredInSequence = true; // Mark as triggered for this pass
                }
                exactMatchFound = true;
                // For an exact match, we generally reset the current sequence
                // as it has been fully consumed by a callback.
            }
        }
    }

    // After checking all callbacks, reset their triggeredInSequence flags for the next evaluation pass
    for (uint8_t i = 0; i < _callbackCount; ++i) {
        _registeredCallbacks[i].triggeredInSequence = false;
    }

    // If an exact match was found, the sequence is "consumed" and should be reset.
    // This allows, for example, S1 to trigger, then S2 to trigger, then S3.
    // If we don't reset, S1 would trigger, then S1+S2 would require an (S,S) callback.
    // The current logic means S triggers S1 callback, then if another S comes, (S,S) triggers S2 callback.
    if (exactMatchFound) {
         _resetSequence();
    } else {
        // If no exact match, but the sequence is full (e.g., S, S, S and no S3 callback)
        // or if it's an LL press (which is always a sequence of 1)
        // then we should reset to allow new sequences.
        if (_currentSequenceCount >= MAX_PRESSES_IN_SEQUENCE ||
            (_currentSequenceCount > 0 && _currentSequence[_currentSequenceCount -1] == ButtonPressType::LL)) {
            _resetSequence();
        }
        // Also, if the current sequence is S,S and there's no S2 callback,
        // but there IS an S1 callback, S1 should have triggered.
        // The current _evaluateSequences iterates all callbacks.
        // The problem is if a user defines S and S,S.
        // Press S: S callback fires. Current sequence is {S}.
        // Press S again: Current sequence becomes {S,S}. S,S callback fires. Sequence resets. This is good.

        // What if user defines S, S,S, S,S,S.
        // Press 1: S fires. Seq={S}.
        // Press 2: S,S fires. Seq={S,S}.
        // Press 3: S,S,S fires. Seq={S,S,S}.
        // The reset on exact match handles this.

        // What if only S,S,S is defined?
        // Press 1: Seq={S}. No exact match. No reset.
        // Press 2: Seq={S,S}. No exact match. No reset.
        // Press 3: Seq={S,S,S}. S,S,S fires. Seq resets. Good.

        // What if only S is defined?
        // Press 1: Seq={S}. S fires. Seq resets. Good.
        // User presses S, S, S.
        // S -> S fires, seq resets.
        // S -> S fires, seq resets.
        // S -> S fires, seq resets.
        // This seems correct. A sequence ends once it matches *a* callback.

        // Exception: LL is always a single event.
        if (_currentSequenceCount == 1 && _currentSequence[0] == ButtonPressType::LL) {
            // LL callback would have fired if registered. Sequence should reset.
            // This is handled by `exactMatchFound` leading to reset,
            // or by the `_currentSequence[_currentSequenceCount -1] == ButtonPressType::LL` condition above.
        }
    }
}


void Button::_resetSequence() {
    _currentSequenceCount = 0;
    for (uint8_t i = 0; i < MAX_PRESSES_IN_SEQUENCE; ++i) {
        _currentSequence[i] = ButtonPressType::NONE;
    }
    // _lastPressEndTime is not reset here, because it's used to see if a *new* press
    // is starting a new sequence or continuing one (within SHORT_PRESS_TIMEOUT).
    // When a sequence fully resets (e.g. due to timeout or completion),
    // the _pressStartTime of the *next* press will be compared to the _lastPressEndTime
    // of the *previous* press that completed/timed-out the sequence.
}

bool Button::onPress(ButtonPressType pressType, ButtonCallback callback) {
    if (pressType == ButtonPressType::NONE) return false;
    // This is a shorthand for a sequence of one.
    return onPressSequence(&pressType, 1, callback);
}

bool Button::onPressSequence(const ButtonPressType types[], uint8_t count, ButtonCallback callback) {
    if (_callbackCount >= MAX_CALLBACKS || count == 0 || count > MAX_PRESSES_IN_SEQUENCE) {
        return false; // No space or invalid sequence
    }

    // Restriction: LL can only be a sequence of 1.
    if (count > 1) {
        for(uint8_t i=0; i < count; ++i) {
            if (types[i] == ButtonPressType::LL) return false; // LL cannot be part of a multi-press sequence definition.
        }
    }
    // Restriction: Sequences must be homogenous (all S or all LS), or a single LL.
    if (count > 1) {
        ButtonPressType firstType = types[0];
        if (firstType == ButtonPressType::LL) return false; // Should have been caught above.
        for (uint8_t i = 1; i < count; ++i) {
            if (types[i] != firstType || types[i] == ButtonPressType::LL) {
                // Heterogeneous sequence (e.g. S, LS) or LL in multi-press is not allowed by problem def.
                return false;
            }
        }
    }


    _registeredCallbacks[_callbackCount].count = count;
    for (uint8_t i = 0; i < count; ++i) {
        _registeredCallbacks[_callbackCount].sequence[i] = types[i];
    }
    for (uint8_t i = count; i < MAX_PRESSES_IN_SEQUENCE; ++i) { // Zero out rest
        _registeredCallbacks[_callbackCount].sequence[i] = ButtonPressType::NONE;
    }
    _registeredCallbacks[_callbackCount].callback = callback;
    _registeredCallbacks[_callbackCount].triggeredInSequence = false;
    _callbackCount++;
    return true;
}