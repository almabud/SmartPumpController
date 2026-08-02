// lib/SystemState/SystemState.h
#pragma once
#include <stdint.h>
#include "config.h"

// The single source of truth for the sensor node.
// Modules that sense something WRITE to the public fields directly.
// Modules that act READ from the public fields.
// No module calls another module directly — everything goes through here.
//
// This is deliberately simpler than the ESP32's SystemState: there is no
// Consumer/Field change-detection machinery, because there is exactly one
// consumer (the radio) and it transmits unconditionally on an interval.

class SystemState {
public:

    // ---- Ranging (written by TankSensor) ---------------------------------
    uint16_t distanceMm     = 0;                // sensor face to water surface
    bool     rangeValid     = false;            // false until the first good echo
    uint32_t lastRangeMs    = 0;                // millis() of the last valid reading

    // ---- Temperature (written by TempSensor) -----------------------------
    // Read by TankSensor to correct the speed of sound. Holds the fallback
    // rather than a stale reading whenever tempValid is false, so the
    // correction always has a usable number to work with.
    float    tempC          = TEMP_FALLBACK_C;  // air temperature above the water
    bool     tempValid      = false;
    uint32_t lastTempMs     = 0;

    // ---- Radio (written by RadioTransmitter) -----------------------------
    uint8_t  txSeq          = 0;                // packet sequence, wraps at 255
    uint32_t txCount        = 0;                // packets sent since boot
    uint32_t lastTxMs       = 0;

    // ---- Uptime (written by the main loop) -------------------------------
    uint32_t uptimeSeconds  = 0;
};
