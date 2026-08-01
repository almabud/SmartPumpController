#include "SceneEngine.h"

void SceneEngine::update(SystemState& state) {
    // A manual request always wins and is consumed here, so it is acted on once.
    if (state.pumpRequest != ActionRequest::NONE) {
        state.desiredPumpAction = state.pumpRequest;
        state.pumpRequest       = ActionRequest::NONE;
    }

    // Phase 4: hysteresis + AUTO mode logic, paused while mode == MANUAL.
    // No AUTO action until PowerMeter and RadioReceiver provide real data.
}
