#include "PowerMeter.h"

void PowerMeter::begin() {
    Serial.println("[PowerMeter] begin (stub — Phase 4)");
}

void PowerMeter::update(SystemState& state) {
    // Phase 4: implement ADC sampling + RMS calculation for ZMPT101B and ACS712
}