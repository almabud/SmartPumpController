#include "PumpDriver.h"
#include "config.h"

void PumpDriver::begin() {
    pinMode(PIN_PUMP_RELAY, OUTPUT);

    // Safe default: pump OFF at boot.
    // Most relay modules are active-LOW (LOW = relay ON, HIGH = relay OFF).
    // If yours is active-HIGH, change HIGH to LOW here and update the
    // comment. Confirm with the relay test in Phase 4.
    digitalWrite(PIN_PUMP_RELAY, HIGH);

    Serial.println("[PumpDriver] begin — pump OFF (relay HIGH)");
}

void PumpDriver::update(SystemState& state) {
    // Phase 4: implement safety rules —
    //   PUMP_MIN_OFF_MS, PUMP_MAX_RUN_MS, stale-data refusal, power-fault refusal
}