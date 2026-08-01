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
    // Phase 4: _runStartTimeMs is used to enforce PUMP_MAX_RUN_MS

    // The single place where relay polarity is resolved.
    void _writeRelay(bool on);
};
