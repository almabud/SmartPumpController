// lib/TankSensor/TankSensor.cpp
#include "TankSensor.h"

void TankSensor::begin() {
    // NewPing sets the pin modes in its constructor, so there is no hardware
    // work left to do here.
    Serial.print(F("[TankSensor] ajsr04m ready ("));
    Serial.print(SENSOR_MIN_CM);
    Serial.print(F("-"));
    Serial.print(SENSOR_MAX_CM);
    Serial.print(F("cm, median of "));
    Serial.print(SENSOR_PING_SAMPLES);
    Serial.println(F(")"));

    // Seed so the first measurement happens on the first pass.
    _lastPingMs = millis() - INTERVAL_RANGE_MS;
}

void TankSensor::update(SystemState& state) {
    uint32_t now = millis();
    if (now - _lastPingMs < INTERVAL_RANGE_MS) return;
    _lastPingMs = now;

    // Median rather than the mean the old sketch used: a single ripple spike
    // no longer drags the reading. NewPing spaces the pings ~29ms apart, so
    // this blocks for roughly 150ms — acceptable, the node has nothing else
    // time-critical to miss, and the scheduling around it is still millis()
    // based. The MAX_DISTANCE passed to the constructor also caps the echo
    // timeout, so a missing echo costs ~26ms instead of pulseIn's 1s default.
    unsigned long echoUs = _sonar.ping_median(SENSOR_PING_SAMPLES);

    if (echoUs == 0) {
        if (state.rangeValid || state.lastRangeMs == 0) {
            Serial.println(F("[TankSensor] no echo - out of range or sensor disconnected"));
        }
        state.rangeValid = false;
        return;
    }

    // Speed of sound in dry air: c = 331.3 + 0.606*T (m/s, T in Celsius).
    // Dividing by 10000 converts m/s to cm/us; halving accounts for the pulse
    // making a round trip. The old sketch hardcoded 0.034 cm/us, which is only
    // right at 20C and drifts about 1.8% per 10C — several centimetres of
    // error over a tall tank between a winter night and a summer afternoon.
    float speedCmPerUs = (331.3f + 0.606f * state.tempC) / 10000.0f;
    float distanceCm   = (echoUs * speedCmPerUs) / 2.0f;

    // The blind zone is a hardware limit, not a full tank. Anything reported
    // from inside it is garbage and must not be passed off as a measurement.
    if (distanceCm < SENSOR_MIN_CM || distanceCm > SENSOR_MAX_CM) {
        if (state.rangeValid || state.lastRangeMs == 0) {
            Serial.print(F("[TankSensor] reading out of bounds - "));
            Serial.print(distanceCm, 1);
            Serial.println(F("cm rejected"));
        }
        state.rangeValid = false;
        return;
    }

    state.distanceMm  = (uint16_t)(distanceCm * 10.0f + 0.5f);
    state.rangeValid  = true;
    state.lastRangeMs = now;
}
