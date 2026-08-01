#include <Arduino.h>
#include <SPI.h>
#include "config.h"

#include "SystemState.h"
#include "DisplayUI.h"
#include "InputManager.h"
#include "PumpDriver.h"
#include "PumpTimer.h"
#include "SceneEngine.h"

// ---- Central state -------------------------------------------------------
SystemState  state;

// ---- Module instances ----------------------------------------------------
DisplayUI    displayUI;
InputManager inputManager;
PumpDriver   pumpDriver;
PumpTimer    pumpTimer;
SceneEngine  sceneEngine;

// ---- Scheduler timestamps ------------------------------------------------
uint32_t lastDisplayMs = 0;

static uint32_t lastUptimeMs = 0;

void setup() {
    // Park the relay output before anything else. The BC547 base pull-down
    // already holds it off through reset, so this is defensive ordering
    // rather than a race being closed.
    pumpDriver.begin();

    Serial.begin(115200);
    delay(300);
    Serial.println("=== pump-controller v2 booting ===");
    pumpDriver.logConfig();

    // Display comes up next — nothing can be shown before it.
    displayUI.begin();
    displayUI.showBoot(1, BOOT_TOTAL_STEPS, "Starting display...");

    inputManager.begin();
    displayUI.showBoot(2, BOOT_TOTAL_STEPS, "Starting inputs...");

    displayUI.showBoot(BOOT_TOTAL_STEPS, BOOT_TOTAL_STEPS, "Ready");
    delay(BOOT_HOLD_MS);   // let the completed bar register before the home screen

    Serial.println("=== ready ===");
}

void loop() {
    uint32_t now = millis();

    // Every iteration — time sensitive
    inputManager.update(state);
    pumpTimer.update(state);     // countdown -> request
    sceneEngine.update(state);   // request  -> desired action
    pumpDriver.update(state);    // desired action -> relay + pumpState

    // Button events drive the display directly so a press is never waiting out
    // the refresh interval; the interval only covers idle repaints.
    ButtonEvent event = inputManager.takeEvent();
    if (event != ButtonEvent::NONE) {
        displayUI.update(state, event);
        lastDisplayMs = now;
    } else if (now - lastDisplayMs >= INTERVAL_DISPLAY_MS) {
        displayUI.update(state, ButtonEvent::NONE);
        lastDisplayMs = now;
    }

    // Uptime counter — increment every second
    if (now - lastUptimeMs >= 1000) {
        state.uptimeSeconds++;
        lastUptimeMs = now;
    }
}