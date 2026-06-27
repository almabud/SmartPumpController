// lib/PumpDriver/PumpDriver.h
#pragma once
#include <Arduino.h>
#include "../SystemState/SystemState.h"

class PumpDriver {
public:
    void begin();
    void update(SystemState& state);

private:
    uint32_t _lastOffTimeMs  = 0;   // timestamp when pump last turned OFF
    uint32_t _runStartTimeMs = 0;   // timestamp when pump last turned ON
    // Phase 4: these are used to enforce PUMP_MIN_OFF_MS and PUMP_MAX_RUN_MS
};