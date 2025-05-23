#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

// Maximum number of presses in a sequence (e.g., S3, LS3)
#define MAX_PRESSES_IN_SEQUENCE 3
// Maximum number of sequences that can be registered
#define MAX_CALLBACKS 10
// Debounce delay in milliseconds
#define DEBOUNCE_DELAY 50
// Time to distinguish between short presses in a sequence (ms)
#define SHORT_PRESS_TIMEOUT 500
// Short press max duration (ms)
#define SHORT_PRESS_MAX_DURATION (SHORT_PRESS_TIMEOUT - 1) // Must be less than timeout
// Longish press min duration (ms)
#define LONGISH_PRESS_MIN_DURATION 1000
// Longish press max duration (ms)
#define LONGISH_PRESS_MAX_DURATION 2500
// Long-Long press min duration (ms)
#define LONG_LONG_PRESS_MIN_DURATION 4000


// Enum to represent different press types
enum class ButtonPressType {
    NONE,
    S,  // Short Press
    LS, // Longish Press
    LL  // Long-Long Press
};

// Type alias for the callback function
typedef void (*ButtonCallback)();

struct ButtonSequence {
    ButtonPressType sequence[MAX_PRESSES_IN_SEQUENCE];
    uint8_t count; // Number of presses in this specific sequence
    ButtonCallback callback;
    bool triggeredInSequence; // To prevent multiple triggers during a sequence evaluation
};

class Button {
public:
    Button();
    void attach(uint8_t pin, uint8_t inputMode = INPUT_PULLUP);
    void update();

    // Methods to add callbacks
    // For single press types (e.g., one long-long press)
    bool onPress(ButtonPressType pressType, ButtonCallback callback);
    // For sequences of presses (e.g., S, S, S or LS, LS)
    bool onPressSequence(const ButtonPressType types[], uint8_t count, ButtonCallback callback);

private:
    uint8_t _pin;
    uint8_t _inputMode;
    bool _buttonPressedState; // Physical state of the button (HIGH/LOW)
    bool _lastButtonState;    // Previous physical state for change detection
    bool _buttonState;        // Debounced button state (true for pressed)
    bool _lastDebouncedState; // Last debounced state

    unsigned long _lastDebounceTime;
    unsigned long _pressStartTime;
    unsigned long _releaseTime;

    ButtonPressType _currentSequence[MAX_PRESSES_IN_SEQUENCE];
    uint8_t _currentSequenceCount;
    unsigned long _lastPressEndTime; // Time when the last press in a potential sequence ended

    ButtonSequence _registeredCallbacks[MAX_CALLBACKS];
    uint8_t _callbackCount;

    void _resetSequence();
    void _addPressToSequence(ButtonPressType pressType);
    void _evaluateSequences();
    ButtonPressType _determinePressType(unsigned long pressDuration);
};

#endif // BUTTON_H