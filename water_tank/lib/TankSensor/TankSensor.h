// lib/TankSensor/TankSensor.h
#pragma once
#include <Arduino.h>
#include <NewPing.h>
#include "config.h"
#include "../SystemState/SystemState.h"

// Measures the distance from the AJSR04M on the tank lid down to the water
// surface, correcting the speed of sound with the temperature TempSensor
// wrote to SystemState.
//
// This module reports raw distance only. Turning distance into a fill
// percentage needs the tank dimensions, and those live on the ESP32 — see
// pump-controller/include/config.h.

class TankSensor {
public:
    void begin();
    void update(SystemState& state);

private:
    NewPing  _sonar{PIN_SENSOR_TRIG, PIN_SENSOR_ECHO, SENSOR_MAX_CM};
    uint32_t _lastPingMs = 0;
};
