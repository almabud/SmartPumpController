#include "PowerMeter.h"
#include <math.h>
#include "config.h"

// ---- Pin millivolts -> real-world units ----------------------------------
//
// Voltage: the ZMPT101B output is scaled by whatever its trim pot is set to, so
// the whole chain collapses into one measured constant. See the calibration
// procedure in docs/wiringe_guide.md — until it is run, ZMPT_CAL_V_PER_V is a
// placeholder and every voltage below is meaningless.
static constexpr float V_PER_PIN_MV = ZMPT_CAL_V_PER_V / 1000.0f;

// Current: the ACS712-30A gives ACS712_MV_PER_AMP per amp, then the divider
// scales that down before it reaches the pin. 66mV/A * 0.686 = 45.3mV per amp.
static constexpr float A_PER_PIN_MV = 1.0f / (ACS712_MV_PER_AMP * ACS712_DIVIDER_RATIO);

// Ignore zero crossings inside this band. ADC noise sitting on top of a
// waveform that spends time near zero would otherwise register dozens of extra
// crossings per cycle and report a frequency several times too high.
static constexpr int32_t ZERO_CROSS_HYST_MV = 50;

// The phase constant expressed as samples of delay. One mains cycle spans
// (1e6 / MAINS_NOMINAL_HZ) microseconds, and each sample is
// ADC_SAMPLE_INTERVAL_US of that — 40 samples per cycle at the defaults, so a
// degree is 0.111 samples. The sign says which channel to hold back.
static constexpr float PHASE_DELAY_SAMPLES =
    (ZMPT_PHASE_CAL_DEG / 360.0f)
    * ((1000000.0f / MAINS_NOMINAL_HZ) / (float)ADC_SAMPLE_INTERVAL_US);

void PowerMeter::begin() {
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_VOLTAGE_SENSE, ADC_11db);
    analogSetPinAttenuation(PIN_CURRENT_SENSE, ADC_11db);

    _lastSampleUs = micros();
    _lastWindowMs = millis();
    _resetWindow();   // seeds the min/max sentinels before the first sample

    Serial.printf("[PowerMeter] ready - V on GPIO%d, I on GPIO%d, %d samples/s\n",
                  PIN_VOLTAGE_SENSE, PIN_CURRENT_SENSE, 1000000 / ADC_SAMPLE_INTERVAL_US);
}

void PowerMeter::_resetWindow() {
    _sumV2     = 0;
    _sumI2     = 0;
    _sumVI     = 0;
    _samples   = 0;
    _crossings = 0;

    _vMinMv = INT32_MAX;  _vMaxMv = INT32_MIN;
    _iMinMv = INT32_MAX;  _iMaxMv = INT32_MIN;
    _vClipped = 0;        _iClipped = 0;
}

void PowerMeter::update(SystemState& state) {
    uint32_t nowUs = micros();

    if (nowUs - _lastSampleUs >= ADC_SAMPLE_INTERVAL_US) {
        // Advance by a fixed step rather than to now, so the sample rate does
        // not drift with however long each loop pass happened to take.
        _lastSampleUs += ADC_SAMPLE_INTERVAL_US;

        // If something held the loop up for several sample periods, resync
        // instead of trying to catch up. Catching up would fire a burst of
        // samples as fast as the ADC allows, all landing on roughly the same
        // point of the waveform and skewing the RMS for the whole window.
        if (nowUs - _lastSampleUs > ADC_SAMPLE_INTERVAL_US * 4) {
            _lastSampleUs = nowUs;
        }

        int32_t vmv = (int32_t)analogReadMilliVolts(PIN_VOLTAGE_SENSE);
        int32_t imv = (int32_t)analogReadMilliVolts(PIN_CURRENT_SENSE);

        // Track the raw extremes before the DC bias is removed — the ADC
        // ceiling is an absolute limit, so it is the raw sample that matters.
        if (vmv < _vMinMv) _vMinMv = vmv;
        if (vmv > _vMaxMv) _vMaxMv = vmv;
        if (imv < _iMinMv) _iMinMv = imv;
        if (imv > _iMaxMv) _iMaxMv = imv;

        if (vmv >= ADC_CLIP_HIGH_MV || vmv <= ADC_CLIP_LOW_MV) _vClipped++;
        if (imv >= ADC_CLIP_HIGH_MV || imv <= ADC_CLIP_LOW_MV) _iClipped++;

        // Start the trackers on the signal itself. Left at zero they would walk
        // up to the real bias over several windows, and every reading until then
        // would carry the DC component into the RMS.
        if (!_seeded) {
            _vOffsetMv = (float)vmv;
            _iOffsetMv = (float)imv;
            _seeded    = true;
        }

        _vOffsetMv += ((float)vmv - _vOffsetMv) / 1024.0f;
        _iOffsetMv += ((float)imv - _iOffsetMv) / 1024.0f;

        int32_t acV = vmv - (int32_t)lroundf(_vOffsetMv);
        // Direction is applied at the sample so it reaches _sumVI, which is the
        // only accumulator that carries a sign — _sumI2 squares it away, so RMS
        // current is unaffected either way.
        int32_t acI = (imv - (int32_t)lroundf(_iOffsetMv)) * ACS712_DIRECTION;

        _vHistory[_histPos] = acV;
        _iHistory[_histPos] = acI;
        _histPos = (uint8_t)((_histPos + 1) % PHASE_HIST);

        // Only the cross term needs realigning. The squares are unaffected by a
        // time shift, so RMS voltage and current stay measurements of the raw
        // signals rather than of the corrected pair.
        int32_t vForPower = acV;
        int32_t iForPower = acI;
        if (PHASE_DELAY_SAMPLES > 0.0f) {
            iForPower = _delayed(_iHistory, PHASE_DELAY_SAMPLES);
        } else if (PHASE_DELAY_SAMPLES < 0.0f) {
            vForPower = _delayed(_vHistory, -PHASE_DELAY_SAMPLES);
        }

        _sumV2 += (int64_t)acV * acV;
        _sumI2 += (int64_t)acI * acI;
        _sumVI += (int64_t)vForPower * iForPower;
        _samples++;

        if (acV > ZERO_CROSS_HYST_MV && !_vAboveZero) {
            _vAboveZero = true;
            _crossings++;
        } else if (acV < -ZERO_CROSS_HYST_MV && _vAboveZero) {
            _vAboveZero = false;
            _crossings++;
        }
    }

    uint32_t nowMs = millis();
    if (nowMs - _lastWindowMs >= POWER_WINDOW_MS) {
        uint32_t windowMs = nowMs - _lastWindowMs;
        _lastWindowMs = nowMs;

        if (_warmedUp) {
            _publish(state, windowMs);
        } else {
            _warmedUp = true;   // first window only settles the offset trackers
        }
        _resetWindow();
    }
}

int32_t PowerMeter::_delayed(const int32_t* history, float samples) const {
    uint8_t whole = (uint8_t)samples;
    float   frac  = samples - (float)whole;

    // _histPos has already advanced past the newest entry, so the sample taken
    // this pass sits at _histPos - 1 and the delay counts back from there.
    uint8_t newer = (uint8_t)((_histPos + PHASE_HIST - 1 - whole) % PHASE_HIST);
    uint8_t older = (uint8_t)((_histPos + PHASE_HIST - 2 - whole) % PHASE_HIST);

    return (int32_t)lroundf((float)history[newer] * (1.0f - frac)
                          + (float)history[older] * frac);
}

void PowerMeter::_publish(SystemState& state, uint32_t windowMs) {
    if (_samples == 0 || windowMs == 0) return;

    float meanV2 = (float)((double)_sumV2 / _samples);
    float meanI2 = (float)((double)_sumI2 / _samples);
    float meanVI = (float)((double)_sumVI / _samples);

    float vRmsPinMv = sqrtf(meanV2);
    float iRmsPinMv = sqrtf(meanI2);

    float volts = vRmsPinMv * V_PER_PIN_MV;
    float amps  = iRmsPinMv * A_PER_PIN_MV;

    // Real power, not volts * amps. Both channels are read in the same pass,
    // tens of microseconds apart, which is well under a degree of phase error
    // at 50Hz — so the mean of the instantaneous product is a true active-power
    // figure. A pump motor runs at a power factor around 0.7-0.85, and the
    // apparent-power shortcut would overstate it by that much.
    float watts = meanVI * V_PER_PIN_MV * A_PER_PIN_MV;

    // With no mains on the ZMPT the pin still carries a millivolt or two of ADC
    // noise, and V_PER_PIN_MV scales that into a volt or so of phantom mains.
    // Nothing downstream can tell that from a real reading, so clamp it here.
    if (volts < POWER_NOISE_FLOOR_V) {
        volts = 0.0f;
        watts = 0.0f;
    }

    // Below the noise floor the ACS712 is only reporting itself. Zero the power
    // along with the current: otherwise noise multiplied by a live mains
    // voltage shows up as tens of phantom watts with the pump switched off.
    if (amps < POWER_NOISE_FLOOR_A) {
        amps  = 0.0f;
        watts = 0.0f;
    }

    // Two crossings per cycle. Only meaningful with real mains on the sensor —
    // below that the hysteresis band is all that stands between noise and a
    // nonsense reading, so report nothing rather than something wrong.
    float hz = 0.0f;
    if (volts >= POWER_V_MIN) {
        hz = (_crossings / 2.0f) / (windowMs / 1000.0f);
    }

    state.voltage    = volts;
    state.current    = amps;
    state.powerWatts = watts;
    state.frequency  = hz;

    // Clipping is reported whatever POWER_CAL_LOG is set to. It does not look
    // like a fault in the numbers — a clipped waveform just reads low — so a
    // silent one would be calibrated against and then trusted forever.
    if (_vClipped || _iClipped) {
        uint32_t nowMs = millis();
        if (nowMs - _lastClipWarnMs >= POWER_CLIP_WARN_MS) {
            _lastClipWarnMs = nowMs;
            Serial.printf("[PowerMeter] CLIPPING - V:%lu I:%lu samples outside %d-%dmV "
                          "(V %ld..%ldmV, I %ld..%ldmV). Readings are LOW. "
                          "Turn the ZMPT trim pot down.\n",
                          (unsigned long)_vClipped, (unsigned long)_iClipped,
                          ADC_CLIP_LOW_MV, ADC_CLIP_HIGH_MV,
                          (long)_vMinMv, (long)_vMaxMv, (long)_iMinMv, (long)_iMaxMv);
        }
    }

#if POWER_CAL_LOG
    // n is the sample count for the window: it is the first thing to check on
    // real hardware, because every figure on this line is only as good as the
    // rate that produced it.
    // pf is what the phase constant is tuned against: on a resistive load it
    // must read 1.000, and whatever it falls short by is the residual error.
    float apparent = volts * amps;
    float pf       = (apparent > 1.0f) ? (watts / apparent) : 0.0f;

    Serial.printf("[PowerMeter] %6.1fV %5.2fA %6.1fW %4.1fHz pf %5.3f | "
                  "pin %6.1f/%6.1fmV bias %6.0f/%6.0fmV n=%lu\n",
                  volts, amps, watts, hz, pf,
                  vRmsPinMv, iRmsPinMv, _vOffsetMv, _iOffsetMv,
                  (unsigned long)_samples);

    // Headroom, so the trim pot can be set by watching a number rather than by
    // guessing. "head" is how far the nearest peak stayed clear of the ADC
    // limits — aim to leave a few hundred mV, not to squeeze it to nothing.
    int32_t vHead = min(ADC_CLIP_HIGH_MV - _vMaxMv, _vMinMv - ADC_CLIP_LOW_MV);
    int32_t iHead = min(ADC_CLIP_HIGH_MV - _iMaxMv, _iMinMv - ADC_CLIP_LOW_MV);
    Serial.printf("[PowerMeter]   range V %ld..%ldmV (head %ldmV)  "
                  "I %ld..%ldmV (head %ldmV)\n",
                  (long)_vMinMv, (long)_vMaxMv, (long)vHead,
                  (long)_iMinMv, (long)_iMaxMv, (long)iHead);
#endif
}
