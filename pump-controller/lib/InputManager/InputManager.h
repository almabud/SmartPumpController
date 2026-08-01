// lib/InputManager/InputManager.h
#pragma once
#include <Arduino.h>
#include "../SystemState/SystemState.h"

enum class ButtonEvent : uint8_t {
    NONE,
    LEFT_PRESS,
    RIGHT_PRESS,
    UP_PRESS,
    DOWN_PRESS,
    SELECT_PRESS,       // OK / confirm
    SELECT_LONG_PRESS   // held for BUTTON_LONG_PRESS_MS — toggles the pump
};

class InputManager {
public:
    void begin();
    void update(SystemState& state);

    // Non-destructive peek.
    ButtonEvent lastEvent() const { return _lastEvent; }

    // Returns the pending event and clears it. Events are latched until taken,
    // so a press is never lost between two update() calls.
    ButtonEvent takeEvent() {
        ButtonEvent e = _lastEvent;
        _lastEvent    = ButtonEvent::NONE;
        return e;
    }

private:
    // Index into the pin table in the .cpp — order must match.
    enum Button : uint8_t {
        BTN_LEFT,
        BTN_UP,
        BTN_SELECT,
        BTN_RIGHT,
        BTN_DOWN,
        BTN_COUNT
    };

    struct ButtonState {
        uint8_t  lastRaw      = HIGH;   // INPUT_PULLUP: released reads HIGH
        uint8_t  stable       = HIGH;   // debounced level
        uint32_t lastChangeMs = 0;      // when lastRaw last flipped
        uint32_t pressStartMs = 0;      // when the debounced press began
        bool     longFired    = false;  // long press already emitted for this hold
    };

    ButtonState _buttons[BTN_COUNT];
    ButtonEvent _lastEvent = ButtonEvent::NONE;
};
