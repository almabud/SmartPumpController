#pragma once
#include "../SystemState/SystemState.h"
#include "../InputManager/InputManager.h"


class DisplayUI {
public:
    void begin();
    void update(SystemState& state, ButtonEvent event);

private:
    void _drawHomeScreen(SystemState& state);
};