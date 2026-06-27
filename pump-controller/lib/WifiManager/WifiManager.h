#pragma once
#include <Arduino.h>
#include "SystemState.h"

class WifiManager {
public:
    void begin();
    void update(SystemState& state);
};