#pragma once
#include <Arduino.h>
#include "SystemState.h"

class CloudClient {
public:
    void begin();
    void update(SystemState& state);
};