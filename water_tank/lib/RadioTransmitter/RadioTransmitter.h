// lib/RadioTransmitter/RadioTransmitter.h
#pragma once
#include <Arduino.h>
#include <RH_ASK.h>
#include "config.h"
#include "../SystemState/SystemState.h"

// Packs SystemState into a SensorPacket and pushes it out over 433 MHz ASK.
// This is the only file in the project that includes RH_ASK. The link is
// one-way: this node never listens.

class RadioTransmitter {
public:
    void begin();
    void update(SystemState& state);

private:
    RH_ASK   _driver{RF433_BITRATE, PIN_RF433_RX_UNUSED, PIN_RF433_TX, PIN_RF433_PTT_UNUSED};
    uint32_t _lastSendMs = 0;
    bool     _ready      = false;
};
