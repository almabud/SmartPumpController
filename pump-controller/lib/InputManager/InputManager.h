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
    SELECT_PRESS     // OK / confirm
};

class InputManager {
public:
    void begin();
    void update(SystemState& state);
    ButtonEvent lastEvent() const { return _lastEvent; }

private:
    ButtonEvent _lastEvent = ButtonEvent::NONE;
};