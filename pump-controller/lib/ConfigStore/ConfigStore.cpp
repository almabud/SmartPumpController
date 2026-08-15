#include "ConfigStore.h"
#include "config.h"
#include <Preferences.h>

// Same namespace as PowerStats — one namespace for the whole device, keys
// prefixed by the module that owns them. Both are limited to 15 characters by
// NVS itself, which is why "tankempt" is not spelled out.
static const char* NVS_NAMESPACE = "pumpctl";
static const char* KEY_VER       = "cfg.ver";
static const char* KEY_TANK_FULL = "cfg.tankfull";
static const char* KEY_TANK_EMPT = "cfg.tankempt";
static const char* KEY_BYPASS    = "cfg.bypass";

bool ConfigStore::_calibrationValid(uint16_t fullMm, uint16_t emptyMm) {
    if (fullMm  < TANK_MIN_MM || fullMm  > TANK_MAX_MM) return false;
    if (emptyMm < TANK_MIN_MM || emptyMm > TANK_MAX_MM) return false;
    // Empty is the further reading by definition — the sensor looks down at the
    // water, so less water means a longer echo.
    return emptyMm > fullMm;
}

void ConfigStore::begin(SystemState& state) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/true)) {
        // Expected on a device that has never saved. SystemState already holds
        // the config.h defaults, so there is nothing to do and nothing to retry.
        Serial.println("[ConfigStore] no saved config - using defaults");
        return;
    }

    uint8_t ver = prefs.getUChar(KEY_VER, 0);
    if (ver != CFG_NVS_VER) {
        // Either nothing has been saved yet, or the schema moved on. Either way
        // the stored keys cannot be trusted to mean what they used to.
        Serial.printf("[ConfigStore] saved config discarded - version %u (want %d)\n",
                      ver, CFG_NVS_VER);
        prefs.end();
        return;
    }

    uint16_t fullMm  = prefs.getUShort(KEY_TANK_FULL,  state.tankFullMm);
    uint16_t emptyMm = prefs.getUShort(KEY_TANK_EMPT,  state.tankEmptyMm);
    bool     bypass  = prefs.getBool(KEY_BYPASS,       state.bypass);
    prefs.end();

    // A bad pair is left on the defaults rather than half-applied: taking one
    // of the two would produce a plausible-looking span that is wrong.
    if (!_calibrationValid(fullMm, emptyMm)) {
        Serial.printf("[ConfigStore] saved calibration rejected - full %umm, empty %umm "
                      "(valid range %d-%dmm, empty must exceed full)\n",
                      fullMm, emptyMm, TANK_MIN_MM, TANK_MAX_MM);
    } else {
        state.tankFullMm  = fullMm;
        state.tankEmptyMm = emptyMm;
    }

    state.bypass = bypass;

    Serial.printf("[ConfigStore] restored - full %umm, empty %umm, bypass %s\n",
                  state.tankFullMm, state.tankEmptyMm, state.bypass ? "on" : "off");
}

void ConfigStore::update(SystemState& state) {
    if (!state.configDirty) return;

    _save(state);
    state.configDirty = false;
}

void ConfigStore::_save(const SystemState& state) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) {
        Serial.println("[ConfigStore] NVS open failed - config not saved");
        return;
    }

    prefs.putUChar(KEY_VER, CFG_NVS_VER);
    prefs.putUShort(KEY_TANK_FULL, state.tankFullMm);
    prefs.putUShort(KEY_TANK_EMPT, state.tankEmptyMm);
    prefs.putBool(KEY_BYPASS, state.bypass);
    prefs.end();

    // No flush timer, unlike PowerStats: settings change by hand, so there is
    // no wear to spread out and no reason to make the user wait for the write.
    Serial.printf("[ConfigStore] saved - full %umm, empty %umm, bypass %s\n",
                  state.tankFullMm, state.tankEmptyMm, state.bypass ? "on" : "off");
}
