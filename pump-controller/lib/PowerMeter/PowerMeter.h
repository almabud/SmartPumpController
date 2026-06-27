#pragma once
#include <Arduino.h>
#include "../SystemState/SystemState.h"

class PowerMeter {
public:
    void begin();
    void update(SystemState& state);
};