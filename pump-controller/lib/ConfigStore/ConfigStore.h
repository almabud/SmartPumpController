// lib/ConfigStore/ConfigStore.h
#pragma once
#include <Arduino.h>
#include "../SystemState/SystemState.h"

// Owns the user settings that survive a reboot — the tank calibration
// distances and the bypass flag. The config page edits them on SystemState and
// raises state.configDirty; this module is what turns that into a flash write.
class ConfigStore {
public:
    // Loads saved settings over the config.h defaults SystemState starts with.
    // Must run before RadioReceiver, which reads the calibration every packet.
    void begin(SystemState& state);

    // Writes on the pass after a commit, then clears the dirty flag.
    void update(SystemState& state);

private:
    void _save(const SystemState& state);

    // Both distances are checked together: either one out of the sensor's range,
    // or an empty that is not further away than full, makes every level reading
    // nonsense, so the pair is rejected as a pair.
    static bool _calibrationValid(uint16_t fullMm, uint16_t emptyMm);
};
