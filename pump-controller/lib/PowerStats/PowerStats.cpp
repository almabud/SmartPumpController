#include "PowerStats.h"

void PowerStats::begin() {
    _lastTickMs = millis();
    _lastLogMs  = _lastTickMs;

    Serial.printf("[PowerStats] ready - %d buckets of %lus (%luh window)\n",
                  POWER_STATS_BUCKETS,
                  (unsigned long)(POWER_STATS_BUCKET_MS / 1000),
                  (unsigned long)(POWER_STATS_BUCKETS * POWER_STATS_BUCKET_MS / 3600000UL));
}

void PowerStats::update(SystemState& state) {
    uint32_t now = millis();

    // Edges are checked every pass, not on the tick below. A RUN slice from the
    // duty-cycle timer can be shorter than one tick, and a missed edge is a
    // cycle that can never be recovered.
    if (state.pumpState != _prevPumpState) {
        if (state.pumpState == PumpState::ON) {
            if (_buckets[_head].cycles < UINT16_MAX) _buckets[_head].cycles++;
        }
        _prevPumpState = state.pumpState;
    }

    if (now - _lastTickMs < POWER_STATS_TICK_MS) return;

    uint32_t elapsedMs = now - _lastTickMs;
    _lastTickMs = now;

    _accumulate(state, elapsedMs);

    // Roll on elapsed time rather than on a tick count, so a slow or stalled
    // pass cannot quietly stretch an hour.
    _hourAccumMs += elapsedMs;
    while (_hourAccumMs >= POWER_STATS_BUCKET_MS) {
        _hourAccumMs -= POWER_STATS_BUCKET_MS;
        _advanceBucket();
    }

    _recomputeTotals(state);

#if POWER_STATS_LOG_MS
    if (now - _lastLogMs >= POWER_STATS_LOG_MS) {
        _lastLogMs = now;
        _logTotals(state);
    }
#endif
}

void PowerStats::_accumulate(SystemState& state, uint32_t elapsedMs) {
    HourBucket& b = _buckets[_head];

    // Energy is integrated here rather than read off state.energyKwh. That one
    // is a lifetime counter owned by PowerMeter, and deriving a windowed figure
    // from its delta would break the moment anything reset or reloaded it.
    _energyRemainderMilliWh += state.powerWatts * (float)elapsedMs / 3600.0f;
    if (_energyRemainderMilliWh >= 1.0f) {
        uint32_t whole = (uint32_t)_energyRemainderMilliWh;
        b.energyMilliWh         += whole;
        _energyRemainderMilliWh -= (float)whole;
    }

    if (state.pumpState != PumpState::ON) return;

    b.runtimeSec += (elapsedMs + 500) / 1000;   // nearest second

    // Current is only sampled while the pump runs. Averaging the idle hours in
    // would divide by 24 and bury the slow upward drift that makes this number
    // worth having in the first place.
    uint32_t mA = (uint32_t)(state.current * 1000.0f);
    if (mA > UINT16_MAX) mA = UINT16_MAX;

    b.currentSumMa += mA;
    if (b.currentSamples < UINT16_MAX) b.currentSamples++;
    if (mA > b.peakCurrentMa) b.peakCurrentMa = (uint16_t)mA;
}

void PowerStats::_advanceBucket() {
    // The slot rolling in is the one rolling out of the window, so clearing it
    // is what actually drops the oldest hour from every total.
    _head = (_head + 1) % POWER_STATS_BUCKETS;
    _buckets[_head] = HourBucket{};
}

void PowerStats::_recomputeTotals(SystemState& state) {
    uint64_t energyMilliWh  = 0;
    uint64_t currentSumMa   = 0;
    uint32_t runtimeSec     = 0;
    uint32_t cycles         = 0;
    uint32_t currentSamples = 0;
    uint16_t peakMa         = 0;

    for (uint8_t i = 0; i < POWER_STATS_BUCKETS; i++) {
        const HourBucket& b = _buckets[i];
        energyMilliWh  += b.energyMilliWh;
        currentSumMa   += b.currentSumMa;
        runtimeSec     += b.runtimeSec;
        cycles         += b.cycles;
        currentSamples += b.currentSamples;
        if (b.peakCurrentMa > peakMa) peakMa = b.peakCurrentMa;
    }

    state.stats24hEnergyKwh   = (float)energyMilliWh / 1000000.0f;
    state.stats24hRuntimeSec  = runtimeSec;
    state.stats24hCycles      = (uint16_t)(cycles > UINT16_MAX ? UINT16_MAX : cycles);
    state.stats24hPeakCurrent = peakMa / 1000.0f;
    state.stats24hAvgCurrent  = currentSamples
                              ? ((float)currentSumMa / currentSamples) / 1000.0f
                              : 0.0f;
}

void PowerStats::_logTotals(const SystemState& state) const {
    Serial.printf("[PowerStats] 24h - run %lu:%02lu:%02lu cyc %u %.3fkWh "
                  "avg %.2fA pk %.2fA | bucket %u/%d\n",
                  (unsigned long)(state.stats24hRuntimeSec / 3600),
                  (unsigned long)((state.stats24hRuntimeSec % 3600) / 60),
                  (unsigned long)(state.stats24hRuntimeSec % 60),
                  state.stats24hCycles,
                  state.stats24hEnergyKwh,
                  state.stats24hAvgCurrent,
                  state.stats24hPeakCurrent,
                  _head, POWER_STATS_BUCKETS);
}
