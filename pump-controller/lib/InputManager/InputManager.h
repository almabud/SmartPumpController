// lib/InputManager/InputManager.h
#pragma once
#include <Arduino.h>
#include "../SystemState/SystemState.h"

enum class ButtonEvent : uint8_t {
    NONE,
    UP_PRESS,
    DOWN_PRESS,
    SELECT_PRESS,
    BACK_PRESS,
    BACK_LONGPRESS   // long-press BACK = return to home screen from anywhere
};

class InputManager {
public:
    void begin();
    void update(SystemState& state);
    ButtonEvent lastEvent() const { return _lastEvent; }

private:
    ButtonEvent _lastEvent = ButtonEvent::NONE;
};