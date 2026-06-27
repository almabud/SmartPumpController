#pragma once
#include <Arduino.h>
#include "SystemState.h"

class RadioReceiver {
public:
    void begin();
    void update(SystemState& state);
};