// lib/TempSensor/TempSensor.h
#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"
#include "../SystemState/SystemState.h"

// Reads the DS18B20 mounted next to the ultrasonic sensor. This is the air
// temperature in the gap above the water, not the water temperature — it
// exists so TankSensor can correct the speed of sound.

enum class TempPhase : uint8_t {
    IDLE,        // waiting out INTERVAL_TEMP_MS
    CONVERTING   // conversion requested, waiting TEMP_CONVERSION_MS for the result
};

class TempSensor {
public:
    void begin();
    void update(SystemState& state);

private:
    OneWire           _wire{PIN_TEMP_DATA};
    DallasTemperature _dallas{&_wire};

    TempPhase _phase          = TempPhase::IDLE;
    uint32_t  _lastRequestMs  = 0;
    bool      _presentAtBoot  = false;
};
