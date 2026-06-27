#include "SystemState.h"
#include <math.h>   // for fabsf

// Helper: are two floats different enough to count as a change?
// Using a small epsilon avoids spurious redraws from floating-point noise.
static bool floatChanged(float a, float b) {
    return fabsf(a - b) > 0.01f;
}

bool SystemState::hasChanged(Consumer consumer) const {
    const StateSnapshot& s = _snapshots[static_cast<uint8_t>(consumer)];
    return
        tankLevelPct    != s.tankLevelPct    ||
        tankStale       != s.tankStale       ||
        pumpState       != s.pumpState       ||
        mode            != s.mode            ||
        powerFault      != s.powerFault      ||
        pumpFault       != s.pumpFault       ||
        wifiConnected   != s.wifiConnected   ||
        cloudConnected  != s.cloudConnected  ||
        activeSceneId   != s.activeSceneId   ||
        uptimeSeconds   != s.uptimeSeconds   ||
        floatChanged(voltage,     s.voltage)     ||
        floatChanged(current,     s.current)     ||
        floatChanged(powerWatts,  s.powerWatts)  ||
        floatChanged(energyKwh,   s.energyKwh)   ||
        floatChanged(frequency,   s.frequency);
}

bool SystemState::hasChanged(Consumer consumer, Field field) const {
    const StateSnapshot& s = _snapshots[static_cast<uint8_t>(consumer)];
    switch (field) {
        case Field::TANK_LEVEL:     return tankLevelPct   != s.tankLevelPct;
        case Field::TANK_STALE:     return tankStale      != s.tankStale;
        case Field::PUMP_STATE:     return pumpState      != s.pumpState;
        case Field::OPERATING_MODE: return mode           != s.mode;
        case Field::VOLTAGE:        return floatChanged(voltage,    s.voltage);
        case Field::CURRENT:        return floatChanged(current,    s.current);
        case Field::POWER_WATTS:    return floatChanged(powerWatts, s.powerWatts);
        case Field::ENERGY_KWH:     return floatChanged(energyKwh,  s.energyKwh);
        case Field::FREQUENCY:      return floatChanged(frequency,  s.frequency);
        case Field::POWER_FAULT:    return powerFault     != s.powerFault;
        case Field::PUMP_FAULT:     return pumpFault      != s.pumpFault;
        case Field::WIFI_CONNECTED: return wifiConnected  != s.wifiConnected;
        case Field::CLOUD_CONNECTED:return cloudConnected != s.cloudConnected;
        case Field::ACTIVE_SCENE:   return activeSceneId  != s.activeSceneId;
        case Field::UPTIME:         return uptimeSeconds  != s.uptimeSeconds;
        default:                    return false;
    }
}

void SystemState::markSeen(Consumer consumer) {
    StateSnapshot& s = _snapshots[static_cast<uint8_t>(consumer)];
    s.tankLevelPct   = tankLevelPct;
    s.tankStale      = tankStale;
    s.pumpState      = pumpState;
    s.mode           = mode;
    s.voltage        = voltage;
    s.current        = current;
    s.powerWatts     = powerWatts;
    s.energyKwh      = energyKwh;
    s.frequency      = frequency;
    s.powerFault     = powerFault;
    s.pumpFault      = pumpFault;
    s.wifiConnected  = wifiConnected;
    s.cloudConnected = cloudConnected;
    s.activeSceneId  = activeSceneId;
    s.uptimeSeconds  = uptimeSeconds;
}
