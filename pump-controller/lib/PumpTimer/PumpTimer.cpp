#include "PumpTimer.h"

// Both halves have to exist for there to be a cycle at all.
bool PumpTimer::_hasDuty(const SystemState& state) {
    return state.timerBreakSec > 0 && state.timerRunSec > 0;
}

// Seconds the pump runs before the next break.
uint32_t PumpTimer::_runSlice(const SystemState& state) {
    return state.timerRunSec;
}

void PumpTimer::_arm(SystemState& state) {
    if (state.timerTotalSec == 0) {           // nothing to run
        _clear(state);
        return;
    }

    bool duty = _hasDuty(state);

    state.timerRemainSec = state.timerTotalSec;
    state.timerPhase     = TimerPhase::RUNNING;
    // A first run slice longer than the whole budget is trimmed to it.
    state.timerPhaseSec  = (duty && _runSlice(state) < state.timerTotalSec)
                         ? _runSlice(state) : state.timerTotalSec;
    state.mode           = OperatingMode::MANUAL;   // keep Phase 4 AUTO logic out of it
    _lastTickMs          = millis();

    Serial.printf("[PumpTimer] armed - %lus window, %s\n",
                  (unsigned long)state.timerTotalSec,
                  duty ? "duty cycle" : "continuous");
}

void PumpTimer::_clear(SystemState& state) {
    state.timerPhase       = TimerPhase::IDLE;
    state.timerRemainSec   = 0;
    state.timerPhaseSec    = 0;
    state.timerTotalSec    = 0;
    state.timerBreakSec    = 0;
    state.timerRunSec      = 0;
    state.mode             = OperatingMode::AUTO;
}

void PumpTimer::update(SystemState& state) {
    if (state.timerRequest != TimerRequest::NONE) {
        TimerRequest req   = state.timerRequest;
        state.timerRequest = TimerRequest::NONE;   // consume

        if (req == TimerRequest::START) {
            _arm(state);
        } else {
            _clear(state);
            state.pumpRequest = ActionRequest::TURN_OFF;
            Serial.println("[PumpTimer] cancelled");
        }
    }

    if (state.timerPhase == TimerPhase::IDLE) return;

    // Advancing by a fixed 1000ms rather than resetting to now() keeps the
    // countdown from drifting when a loop iteration runs long.
    uint32_t now = millis();
    while (now - _lastTickMs >= 1000) {
        _lastTickMs += 1000;

        // The window is a pump-on budget, not wall clock: a break costs it
        // nothing and simply parks it until the pump comes back.
        if (state.timerPhase == TimerPhase::RUNNING && state.timerRemainSec > 0) {
            state.timerRemainSec--;
        }
        if (state.timerPhaseSec > 0) state.timerPhaseSec--;

        if (state.timerPhase == TimerPhase::RUNNING && state.timerRemainSec == 0) {
            _clear(state);
            state.pumpRequest = ActionRequest::TURN_OFF;
            Serial.println("[PumpTimer] window finished - pump OFF");
            return;
        }

        // Slice is up and a duty cycle is configured — swap RUN <-> BREAK.
        if (state.timerPhaseSec == 0 && _hasDuty(state)) {
            bool wasRunning  = (state.timerPhase == TimerPhase::RUNNING);
            state.timerPhase = wasRunning ? TimerPhase::BREAKING : TimerPhase::RUNNING;

            if (wasRunning) {
                state.timerPhaseSec = state.timerBreakSec;   // breaks run their full length
            } else {
                // Only a run slice spends the budget, so only it is capped by
                // what is left of it.
                uint32_t slice      = _runSlice(state);
                state.timerPhaseSec = slice < state.timerRemainSec ? slice : state.timerRemainSec;
            }

            Serial.printf("[PumpTimer] %s for %lus (%lus left in window)\n",
                          wasRunning ? "break" : "run",
                          (unsigned long)state.timerPhaseSec,
                          (unsigned long)state.timerRemainSec);
        }
    }

    // Asserted every pass, not only on a flip: PumpDriver silently refuses a
    // start while PUMP_MIN_OFF_MS is still counting, so a one-shot request at
    // the phase edge could be swallowed and the slice would never start.
    PumpState want = (state.timerPhase == TimerPhase::RUNNING) ? PumpState::ON : PumpState::OFF;
    if (state.pumpState != want && state.pumpRequest == ActionRequest::NONE) {
        state.pumpRequest = (want == PumpState::ON) ? ActionRequest::TURN_ON
                                                    : ActionRequest::TURN_OFF;
    }
}
