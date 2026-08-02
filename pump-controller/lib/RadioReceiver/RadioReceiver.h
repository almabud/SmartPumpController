// lib/RadioReceiver/RadioReceiver.h
#pragma once
#include <Arduino.h>
#include <RH_ASK.h>
#include <SensorPacket.h>
#include "config.h"
#include "SystemState.h"

// Receives SensorPackets from the Nano tank node over 433 MHz ASK and writes
// the tank fields in SystemState. This is the only file in the ESP32 firmware
// that includes RH_ASK. The link is one-way: this node never transmits.

class RadioReceiver {
public:
    void begin();
    void update(SystemState& state);

private:
    void _applyPacket(SystemState& state, const SensorPacket& packet);
    void _logLinkStats();

    RH_ASK _driver{RF433_BITRATE, PIN_RF433_RX, PIN_RF433_TX_UNUSED, PIN_RF433_PTT_UNUSED};
    bool   _ready = false;

    // Link diagnostics. Distinguishes "no RF at all" from "RF arriving but
    // unusable", which is otherwise invisible — a silent link and a miswired
    // one look identical from the outside.
    uint32_t _lastStatsMs  = 0;
    uint32_t _pinSamples   = 0;   // times the RX pin was sampled this window
    uint32_t _pinEdges     = 0;   // times it changed level
    uint16_t _badPackets   = 0;   // RH_ASK frame accepted, SensorPacket rejected
    uint8_t  _lastPinLevel = 0;
};
