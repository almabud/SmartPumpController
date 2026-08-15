#pragma once
#include <Arduino.h>
#include "../SystemState/SystemState.h"
#include "config.h"

// One slot of the rolling window. Deliberately small and plain-old-data: a
// later stage writes the whole ring to NVS as a single blob.
struct HourBucket {
    uint32_t energyMilliWh;
    uint32_t runtimeSec;
    uint32_t currentSumMa;      // summed once per pump-on tick
    uint16_t currentSamples;    // count of those ticks, so the mean is a true mean
    uint16_t cycles;            // OFF -> ON transitions
    uint16_t peakCurrentMa;
};

// Keeps a rolling POWER_STATS_BUCKETS-slot window of what the pump has been
// doing and publishes the aggregate into SystemState for the UI to draw.
//
// Must be updated LAST in the loop pipeline, after PumpDriver, so it observes
// the pump state that this pass actually settled on rather than the previous
// pass's.
//
// There is no wall clock on this board, so buckets advance on elapsed millis()
// rather than on real hour boundaries. The window is therefore "the last 24
// recorded hours"; _advanceBucket() is the single place that changes when NTP
// arrives and real timestamps become available.
class PowerStats {
public:
    void begin();
    void update(SystemState& state);

private:
    HourBucket _buckets[POWER_STATS_BUCKETS] = {};
    uint8_t    _head = 0;

    // Pump edges are counted against a local previous value rather than through
    // SystemState's change detection. That mechanism has exactly two consumers,
    // and adding a third would mean extending StateSnapshot and all three
    // functions in SystemState.cpp — for something one compare does as well.
    PumpState _prevPumpState = PumpState::OFF;

    uint32_t _lastTickMs  = 0;
    uint32_t _hourAccumMs = 0;
    uint32_t _lastLogMs   = 0;

    // Energy arrives as a rate, so the sub-milliwatt-hour remainder is carried
    // between ticks rather than truncated away once a second — at low power
    // that rounding would otherwise swallow the whole reading.
    float _energyRemainderMilliWh = 0.0f;

    void _accumulate(SystemState& state, uint32_t elapsedMs);
    void _advanceBucket();
    void _recomputeTotals(SystemState& state);
    void _logTotals(const SystemState& state) const;
};
