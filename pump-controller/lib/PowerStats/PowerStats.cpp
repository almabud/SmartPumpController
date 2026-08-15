#include "PowerStats.h"
#include <Preferences.h>

// First NVS use in this project, so this sets the convention: one namespace for
// the whole device, keys prefixed by the module that owns them. Both are length
// limited by NVS itself — 15 characters each.
static const char* NVS_NAMESPACE = "pumpctl";
static const char* KEY_VER       = "pwr.ver";
static const char* KEY_BUCKETS   = "pwr.buckets";
static const char* KEY_HEAD      = "pwr.head";
static const char* KEY_ACCUM     = "pwr.accum";
static const char* KEY_KWH       = "pwr.kwh";

static bool bucketEmpty(const HourBucket& b) {
    return b.energyMilliWh == 0 && b.runtimeSec == 0 && b.currentSumMa == 0
        && b.currentSamples == 0 && b.cycles == 0 && b.peakCurrentMa == 0;
}

void PowerStats::begin(SystemState& state) {
    _lastTickMs  = millis();
    _lastLogMs   = _lastTickMs;
    _lastFlushMs = _lastTickMs;

    _load(state);
    _recomputeTotals(state);

    Serial.printf("[PowerStats] ready - %d buckets of %lus (%luh window)\n",
                  POWER_STATS_BUCKETS,
                  (unsigned long)(POWER_STATS_BUCKET_MS / 1000),
                  (unsigned long)(POWER_STATS_BUCKETS * POWER_STATS_BUCKET_MS / 3600000UL));
}

void PowerStats::_load(SystemState& state) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/true)) {
        // Expected on a device that has never saved: the namespace does not
        // exist yet. Not an error, and not something to retry.
        Serial.println("[PowerStats] no saved stats - starting clean");
        return;
    }

    uint8_t ver = prefs.getUChar(KEY_VER, 0);
    size_t  len = prefs.getBytesLength(KEY_BUCKETS);

    // Both guards earn their place. The version catches a deliberate schema
    // change; the length catches a struct that changed size without the version
    // being bumped. Reading a blob of the wrong shape does not fail — it
    // produces plausible-looking nonsense, which is far worse than starting
    // clean.
    if (ver != POWER_STATS_NVS_VER || len != sizeof(_buckets)) {
        Serial.printf("[PowerStats] saved stats discarded - version %u (want %d), "
                      "%u bytes (want %u)\n",
                      ver, POWER_STATS_NVS_VER,
                      (unsigned)len, (unsigned)sizeof(_buckets));
        prefs.end();
        return;
    }

    prefs.getBytes(KEY_BUCKETS, _buckets, sizeof(_buckets));
    _head           = prefs.getUChar(KEY_HEAD, 0);
    _hourAccumMs    = prefs.getULong(KEY_ACCUM, 0);
    state.energyKwh = prefs.getFloat(KEY_KWH, 0.0f);
    prefs.end();

    // A corrupt head would index past the end of the ring on the very first
    // write, so it is range-checked rather than trusted.
    if (_head >= POWER_STATS_BUCKETS) _head = 0;
    if (_hourAccumMs >= POWER_STATS_BUCKET_MS) _hourAccumMs = 0;

    Serial.printf("[PowerStats] restored - head %u, %lums into the bucket, %.3fkWh lifetime\n",
                  _head, (unsigned long)_hourAccumMs, state.energyKwh);
}

void PowerStats::_save(const SystemState& state) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) {
        Serial.println("[PowerStats] NVS open failed - stats not saved");
        return;
    }

    prefs.putUChar(KEY_VER, POWER_STATS_NVS_VER);
    prefs.putBytes(KEY_BUCKETS, _buckets, sizeof(_buckets));
    prefs.putUChar(KEY_HEAD, _head);
    prefs.putULong(KEY_ACCUM, _hourAccumMs);
    // Lifetime energy belongs to PowerMeter, but PowerStats is the only module
    // with NVS open, so it carries it. Until the energy stage lands this
    // persists a zero, and then starts working with no further change.
    prefs.putFloat(KEY_KWH, state.energyKwh);
    prefs.end();

    _dirty = false;
    _writeCount++;
}

void PowerStats::update(SystemState& state) {
    uint32_t now = millis();

    // Edges are checked every pass, not on the tick below. A RUN slice from the
    // duty-cycle timer can be shorter than one tick, and a missed edge is a
    // cycle that can never be recovered.
    if (state.pumpState != _prevPumpState) {
        if (state.pumpState == PumpState::ON) {
            if (_buckets[_head].cycles < UINT16_MAX) _buckets[_head].cycles++;
            _dirty = true;
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

    if (_dirty && now - _lastFlushMs >= POWER_STATS_FLUSH_MS) {
        _lastFlushMs = now;
        _save(state);
    }

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
        _dirty = true;
    }

    if (state.pumpState != PumpState::ON) return;

    b.runtimeSec += (elapsedMs + 500) / 1000;   // nearest second
    _dirty = true;

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

    // Dropping data is a change worth persisting, even though nothing was
    // added. Without this, a pump that ran and then sat idle long enough for
    // the window to empty would come back from a reboot with the old figures
    // restored — data that had already aged out, resurrected by a save that
    // never happened.
    if (!bucketEmpty(_buckets[_head])) _dirty = true;

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
                  "avg %.2fA pk %.2fA | bucket %u/%d writes=%lu%s\n",
                  (unsigned long)(state.stats24hRuntimeSec / 3600),
                  (unsigned long)((state.stats24hRuntimeSec % 3600) / 60),
                  (unsigned long)(state.stats24hRuntimeSec % 60),
                  state.stats24hCycles,
                  state.stats24hEnergyKwh,
                  state.stats24hAvgCurrent,
                  state.stats24hPeakCurrent,
                  _head, POWER_STATS_BUCKETS,
                  (unsigned long)_writeCount, _dirty ? " dirty" : "");
}
