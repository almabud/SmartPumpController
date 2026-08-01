// lib/PumpTimer/PumpTimer.h
#pragma once
#include <Arduino.h>
#include "SystemState.h"

// Owns the pump run window: counts it down, flips between the RUN and BREAK
// slices inside it, and asks for the pump via state.pumpRequest — which
// SceneEngine consumes, so this module never touches the relay itself.
class PumpTimer {
public:
    void update(SystemState& state);

private:
    void _arm(SystemState& state);
    void _clear(SystemState& state);
    static bool     _hasDuty(const SystemState& state);
    static uint32_t _runSlice(const SystemState& state);

    uint32_t _lastTickMs = 0;
};
