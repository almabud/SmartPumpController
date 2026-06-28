#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../SystemState/SystemState.h"
#include "../InputManager/InputManager.h"

class DisplayUI {
public:
    void begin();
    void update(SystemState& state, ButtonEvent event);
};
