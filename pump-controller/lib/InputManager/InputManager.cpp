#include "InputManager.h"
#include "config.h"

// Order must match the Button enum in the header.
static const uint8_t PINS[] = {
    PIN_BTN_LEFT,
    PIN_BTN_UP,
    PIN_BTN_SELECT,
    PIN_BTN_RIGHT,
    PIN_BTN_DOWN
};

// Short-press event per button, same order. Emitted on release.
static const ButtonEvent SHORT_EVENTS[] = {
    ButtonEvent::LEFT_PRESS,
    ButtonEvent::UP_PRESS,
    ButtonEvent::SELECT_PRESS,
    ButtonEvent::RIGHT_PRESS,
    ButtonEvent::DOWN_PRESS
};

void InputManager::begin() {
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        pinMode(PINS[i], INPUT_PULLUP);
    }
    Serial.println("[InputManager] begin - 5 buttons ready");
}

void InputManager::update(SystemState& state) {
    // _lastEvent is NOT cleared here — it is latched until takeEvent() collects
    // it. Clearing per call would drop nearly every press, since the consumer
    // only polls on its own (much slower) cadence.
    uint32_t now  = millis();

    // Buttons are INPUT_PULLUP, so pressed reads LOW.
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        ButtonState& b  = _buttons[i];
        uint8_t      raw = digitalRead(PINS[i]);

        // Any edge restarts the bounce window; it is not a state change yet.
        if (raw != b.lastRaw) {
            b.lastRaw      = raw;
            b.lastChangeMs = now;
        }

        // Level has held steady long enough to be believed.
        if (raw != b.stable && (now - b.lastChangeMs) >= BUTTON_DEBOUNCE_MS) {
            b.stable = raw;

            if (raw == LOW) {                   // press
                b.pressStartMs = now;
                b.longFired    = false;
            } else {                            // release
                // A completed hold must not also fire a short press, so short
                // presses are emitted on release rather than on press.
                if (!b.longFired) _lastEvent = SHORT_EVENTS[i];
            }
        }

        // Held long enough — fire once, mid-hold.
        if (b.stable == LOW && !b.longFired &&
            (now - b.pressStartMs) >= BUTTON_LONG_PRESS_MS) {
            b.longFired = true;

            if (i == BTN_SELECT) {
                _lastEvent = ButtonEvent::SELECT_LONG_PRESS;

                if (state.uiEditing) {
                    // The edit UI owns the buttons — it uses the long press to
                    // back out, and must not disturb whatever is running.
                } else if (state.timerPhase != TimerPhase::IDLE) {
                    // A running timer owns the pump, so the override is to end
                    // the timer — PumpTimer turns the pump off as it clears.
                    state.timerRequest = TimerRequest::CANCEL;
                    Serial.println("[InputManager] SELECT long press - timer cancelled");
                } else {
                    // Manual pump override. SceneEngine consumes the request;
                    // MANUAL keeps Phase 4's AUTO logic from undoing it.
                    state.pumpRequest = (state.pumpState == PumpState::ON)
                                      ? ActionRequest::TURN_OFF
                                      : ActionRequest::TURN_ON;
                    state.mode = OperatingMode::MANUAL;

                    Serial.printf("[InputManager] SELECT long press - pump %s requested\n",
                                  state.pumpRequest == ActionRequest::TURN_ON ? "ON" : "OFF");
                }
            }
        }
    }
}
