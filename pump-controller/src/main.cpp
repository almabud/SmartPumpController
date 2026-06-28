// Thin by design. Initialises every module, then runs a non-blocking
// millis()-based scheduler. No delay() anywhere in this file or any module.
// All logic lives in individual modules — this file just wires them together.

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

#include "SystemState.h"
#include "RadioReceiver.h"
#include "PowerMeter.h"
#include "InputManager.h"
#include "WifiManager.h"
#include "CloudClient.h"
#include "SceneEngine.h"
#include "PumpDriver.h"
#include "DisplayUI.h"

// ---- Central state -------------------------------------------------------
SystemState state;

// ---- Module instances ----------------------------------------------------
RadioReceiver  radioReceiver;
PowerMeter     powerMeter;
InputManager   inputManager;
WifiManager    wifiManager;
CloudClient    cloudClient;
SceneEngine    sceneEngine;
PumpDriver     pumpDriver;
DisplayUI      displayUI;

// ---- Scheduler timestamps ------------------------------------------------
uint32_t lastRadioMs   = 0;
uint32_t lastPowerMs   = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastWifiMs    = 0;
uint32_t lastCloudMs   = 0;
uint32_t lastSceneMs   = 0;

// -------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);   // settle time for serial monitor — boot only, not in loop
    Serial.println("=== pump-controller v2 booting ===");

    radioReceiver.begin();
    powerMeter.begin();
    inputManager.begin();
    wifiManager.begin();
    cloudClient.begin();
    pumpDriver.begin();
    displayUI.begin();

    Serial.println("=== all modules initialised ===");
}

void loop() {
    uint32_t now = millis();

    // --- Every iteration: time-sensitive modules --------------------------
    inputManager.update(state);     // buttons need fast polling
    pumpDriver.update(state);       // safety timers must stay sharp

    // --- Radio: fast poll -------------------------------------------------
    if (now - lastRadioMs >= INTERVAL_RADIO_MS) {
        radioReceiver.update(state);
        lastRadioMs = now;
    }

    // --- Power meter: 1 second --------------------------------------------
    if (now - lastPowerMs >= INTERVAL_POWER_MS) {
        powerMeter.update(state);
        lastPowerMs = now;
    }

    // --- Wi-Fi health check -----------------------------------------------
    if (now - lastWifiMs >= INTERVAL_WIFI_CHECK_MS) {
        wifiManager.update(state);
        lastWifiMs = now;
    }

    // --- MQTT publish / receive -------------------------------------------
    if (now - lastCloudMs >= INTERVAL_CLOUD_MS) {
        cloudClient.update(state);
        lastCloudMs = now;
    }

    // --- Scene engine: decision layer -------------------------------------
    if (now - lastSceneMs >= 200) {
        sceneEngine.update(state);
        lastSceneMs = now;
    }

    // --- Display: slow refresh --------------------------------------------
    if (now - lastDisplayMs >= INTERVAL_DISPLAY_MS) {
        displayUI.update(state, inputManager.lastEvent());
        lastDisplayMs = now;
    }
}