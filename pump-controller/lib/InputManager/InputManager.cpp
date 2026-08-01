#include "InputManager.h"
#include "config.h"

void InputManager::begin() {
    pinMode(PIN_BTN_LEFT,   INPUT_PULLUP);
    pinMode(PIN_BTN_UP,     INPUT_PULLUP);
    pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
    pinMode(PIN_BTN_RIGHT,  INPUT_PULLUP);
    pinMode(PIN_BTN_DOWN,   INPUT_PULLUP);
    Serial.println("[InputManager] begin — 5 buttons ready");
}

void InputManager::update(SystemState& state) {
    // Phase 2: implement debounce (BUTTON_DEBOUNCE_MS) + long-press detection
    _lastEvent = ButtonEvent::NONE;
}