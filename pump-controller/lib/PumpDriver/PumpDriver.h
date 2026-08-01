// lib/PumpDriver/PumpDriver.h
#pragma once
#include <Arduino.h>
#include "../SystemState/SystemState.h"

class PumpDriver {
public:
    // Parks the relay OFF. Call as the very first thing in setup(), before
    // Serial — the relay is energised from reset until this runs.
    void begin();
    // Diagnostics, split out so begin() need not wait for Serial to be up.
    void logConfig();
    void update(SystemState& state);

private:
    uint32_t _lastOffTimeMs  = 0;   // timestamp when pump last turned OFF
    uint32_t _runStartTimeMs = 0;   // timestamp when pump last turned ON
    // Phase 4: _runStartTimeMs is used to enforce PUMP_MAX_RUN_MS

    // The single place where relay polarity is resolved.
    void _writeRelay(bool on);
};
