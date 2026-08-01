#include <Arduino.h>
#include <SPI.h>
#include "config.h"

#include "SystemState.h"
#include "DisplayUI.h"
#include "InputManager.h"

// ---- Central state -------------------------------------------------------
SystemState  state;

// ---- Module instances ----------------------------------------------------
DisplayUI    displayUI;
InputManager inputManager;

// ---- Scheduler timestamps ------------------------------------------------
uint32_t lastDisplayMs = 0;

static uint32_t lastUptimeMs = 0;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("=== pump-controller v2 booting ===");

    // Display comes up first — nothing can be shown before it.
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

    // Display refresh — 500ms cadence
    if (now - lastDisplayMs >= INTERVAL_DISPLAY_MS) {
        displayUI.update(state, inputManager.lastEvent());
        lastDisplayMs = now;
    }

    // Uptime counter — increment every second
    if (now - lastUptimeMs >= 1000) {
        state.uptimeSeconds++;
        lastUptimeMs = now;
    }
}