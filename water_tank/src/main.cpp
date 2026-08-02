#include <Arduino.h>
#include "config.h"

#include "SystemState.h"
#include "TempSensor.h"
#include "TankSensor.h"
#include "RadioTransmitter.h"

// ---- Central state -------------------------------------------------------
SystemState      state;

// ---- Module instances ----------------------------------------------------
TempSensor       tempSensor;
TankSensor       tankSensor;
RadioTransmitter radio;

// ---- Scheduler timestamps ------------------------------------------------
static uint32_t lastUptimeMs = 0;
static uint32_t lastLogMs    = 0;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.print(F("=== water-tank "));
    Serial.print(F(FIRMWARE_VERSION));
    Serial.println(F(" booting ==="));

    tempSensor.begin();
    tankSensor.begin();
    radio.begin();

    Serial.println(F("=== ready ==="));
}

void loop() {
    uint32_t now = millis();

    // Order matters: temperature first, because TankSensor corrects the speed
    // of sound against state.tempC in this same pass and would otherwise be
    // working from a value one cycle old.
    tempSensor.update(state);   // ds18b20 -> state.tempC
    tankSensor.update(state);   // echo + tempC -> state.distanceMm
    radio.update(state);        // state -> SensorPacket -> 433 MHz

    // Uptime counter — increment every second
    if (now - lastUptimeMs >= 1000) {
        state.uptimeSeconds++;
        lastUptimeMs = now;
    }

    // Diagnostics, so the node can be brought up on a serial monitor alone
    // without a working receiver on the other end.
    if (now - lastLogMs >= INTERVAL_LOG_MS) {
        lastLogMs = now;
        Serial.print(F("[main] dist="));
        Serial.print(state.distanceMm);
        Serial.print(F("mm temp="));
        Serial.print(state.tempC, 1);
        Serial.print(F("C valid="));
        Serial.print(state.rangeValid ? 1 : 0);
        Serial.print(F("/"));
        Serial.print(state.tempValid ? 1 : 0);
        Serial.print(F(" tx="));
        Serial.println(state.txCount);
    }
}
