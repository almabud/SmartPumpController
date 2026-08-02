// lib/TempSensor/TempSensor.cpp
#include "TempSensor.h"

void TempSensor::begin() {
    _dallas.begin();
    _dallas.setResolution(TEMP_RESOLUTION_BITS);

    // The default blocking mode would stall the loop for the full 750 ms
    // conversion. Asking for the reading and collecting it a cycle later is
    // what lets the scheduler stay honest.
    _dallas.setWaitForConversion(false);

    _presentAtBoot = (_dallas.getDeviceCount() > 0);
    if (!_presentAtBoot) {
        // Almost always a missing or wrong-value pull-up on the data line —
        // the Nano's internal 20-50k is too weak for 1-Wire.
        Serial.println(F("[TempSensor] no device on the 1-wire bus - check the 4.6k pull-up"));
    } else {
        Serial.println(F("[TempSensor] ds18b20 found"));
    }

    // Seed so the first conversion is requested immediately rather than after
    // a full INTERVAL_TEMP_MS of running on the fallback temperature.
    _lastRequestMs = millis() - INTERVAL_TEMP_MS;
}

void TempSensor::update(SystemState& state) {
    uint32_t now = millis();

    if (_phase == TempPhase::IDLE) {
        if (now - _lastRequestMs < INTERVAL_TEMP_MS) return;

        _dallas.requestTemperatures();
        _lastRequestMs = now;
        _phase = TempPhase::CONVERTING;
        return;
    }

    // CONVERTING — the sensor needs the full conversion window before the
    // scratchpad holds anything but the previous reading.
    if (now - _lastRequestMs < TEMP_CONVERSION_MS) return;

    float reading = _dallas.getTempCByIndex(0);
    _phase = TempPhase::IDLE;

    // DEVICE_DISCONNECTED_C is -127. Exactly 85.0 is the power-on reset value
    // the DS18B20 reports when a conversion never completed, which is what a
    // marginal pull-up looks like from up here — treat both as a failure.
    bool ok = (reading != DEVICE_DISCONNECTED_C) && (reading != TEMP_RESET_VALUE_C);

    if (!ok) {
        if (state.tempValid || state.lastTempMs == 0) {
            Serial.print(F("[TempSensor] read failed - using fallback "));
            Serial.print(TEMP_FALLBACK_C, 1);
            Serial.println(F("C"));
        }
        state.tempC     = TEMP_FALLBACK_C;
        state.tempValid = false;
        return;
    }

    state.tempC      = reading;
    state.tempValid  = true;
    state.lastTempMs = now;
}
