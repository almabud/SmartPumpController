#pragma once
#include <Arduino.h>
#include "../SystemState/SystemState.h"

// Samples the ZMPT101B voltage sensor and the ACS712 current sensor and
// publishes RMS voltage, RMS current, real power and mains frequency once per
// POWER_WINDOW_MS.
//
// update() must be called on EVERY loop pass. It takes at most one sample pair
// per pass, gated on micros(), and accumulates across passes — a blocking burst
// would stall the display and break the no-delay rule. The loop runs far faster
// than ADC_SAMPLE_INTERVAL_US, so the effective rate is set by that constant
// rather than by the loop.
class PowerMeter {
public:
    void begin();
    void update(SystemState& state);

private:
    // Accumulators for the window in progress. Held in integer millivolts at
    // the pin: acV peaks around 1650, so acV*acV fits an int32 and the running
    // sums fit an int64 exactly, with none of the drift that float accumulation
    // over thousands of terms would introduce.
    int64_t  _sumV2   = 0;
    int64_t  _sumI2   = 0;
    int64_t  _sumVI   = 0;
    uint32_t _samples = 0;

    // Each channel's DC bias, tracked rather than assumed. The ACS712 idles at
    // 2.5V * 0.686 = 1715mV and the ZMPT wherever its trim pot puts it, and both
    // drift with temperature and supply — a hardcoded midpoint would show up as
    // a phantom current that grows as the board warms up.
    float _vOffsetMv = 0.0f;
    float _iOffsetMv = 0.0f;
    bool  _seeded    = false;

    // Mains frequency, counted off the voltage waveform's zero crossings.
    bool     _vAboveZero = false;
    uint32_t _crossings  = 0;

    // Raw extremes and clip counts for the window, per channel. A clipped peak
    // does not look like an error — it just drags the RMS down — so it has to
    // be caught at the sample, not inferred from the result. These are also
    // what makes the voltage trim pot tunable: they say how much headroom is
    // left before the waveform hits the ADC ceiling.
    int32_t  _vMinMv = 0, _vMaxMv = 0;
    int32_t  _iMinMv = 0, _iMaxMv = 0;
    uint32_t _vClipped = 0, _iClipped = 0;
    uint32_t _lastClipWarnMs = 0;

    uint32_t _lastSampleUs = 0;
    uint32_t _lastWindowMs = 0;

    // The offset trackers need a few hundred samples to settle, so the first
    // window is accumulated and thrown away rather than published as a reading.
    bool _warmedUp = false;

    void _publish(SystemState& state, uint32_t windowMs);
    void _resetWindow();
};
