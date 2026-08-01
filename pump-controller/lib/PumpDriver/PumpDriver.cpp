#include "PumpDriver.h"
#include "config.h"

void PumpDriver::_writeRelay(bool on) {
#if RELAY_ACTIVE_LOW
    digitalWrite(PIN_PUMP_RELAY, on ? LOW : HIGH);
#else
    digitalWrite(PIN_PUMP_RELAY, on ? HIGH : LOW);
#endif
}

void PumpDriver::begin() {
    pinMode(PIN_PUMP_RELAY, OUTPUT);

    // Safe default: pump OFF at boot. If the relay audibly clicks here, the
    // module is the opposite polarity — flip RELAY_ACTIVE_LOW in config.h.
    _writeRelay(false);

    // Without this seed the interlock in update() would block the first start
    // for PUMP_MIN_OFF_MS after every boot, since _lastOffTimeMs starts at 0.
    _lastOffTimeMs = millis() - PUMP_MIN_OFF_MS;

    Serial.printf("[PumpDriver] begin - pump OFF (relay active-%s)\n",
                  RELAY_ACTIVE_LOW ? "LOW" : "HIGH");
}

void PumpDriver::update(SystemState& state) {
    if (state.desiredPumpAction == ActionRequest::NONE) return;

    bool wantOn = (state.desiredPumpAction == ActionRequest::TURN_ON);
    state.desiredPumpAction = ActionRequest::NONE;   // consume

    uint32_t now = millis();

    if (wantOn && state.pumpState == PumpState::OFF) {
        // Safety: never restart before the motor has rested.
        if (now - _lastOffTimeMs < PUMP_MIN_OFF_MS) {
            Serial.printf("[PumpDriver] start refused - %lus of %lus min off time left\n",
                          (unsigned long)((PUMP_MIN_OFF_MS - (now - _lastOffTimeMs)) / 1000),
                          (unsigned long)(PUMP_MIN_OFF_MS / 1000));
            return;
        }
        _writeRelay(true);
        _runStartTimeMs = now;
        state.pumpState = PumpState::ON;
        Serial.println("[PumpDriver] pump ON");

    } else if (!wantOn && state.pumpState == PumpState::ON) {
        _writeRelay(false);
        _lastOffTimeMs  = now;
        state.pumpState = PumpState::OFF;
        Serial.println("[PumpDriver] pump OFF");
    }

    // Phase 4: PUMP_MAX_RUN_MS, stale-tank refusal, power-fault refusal
}
