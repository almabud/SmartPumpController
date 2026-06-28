#include <Arduino.h>
#include "SystemState.h"
#include "DisplayUI.h"
#include "InputManager.h"

SystemState  state;
DisplayUI    displayUI;
InputManager inputManager;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("=== pump-controller booting ===");

    inputManager.begin();
    displayUI.begin();

    Serial.println("=== ready ===");
}

void loop() {
    inputManager.update(state);
    displayUI.update(state, inputManager.lastEvent());
    delay(100);
}
