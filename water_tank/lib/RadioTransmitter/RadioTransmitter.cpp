// lib/RadioTransmitter/RadioTransmitter.cpp
#include "RadioTransmitter.h"
#include <SensorPacket.h>

void RadioTransmitter::begin() {
    _ready = _driver.init();
    if (!_ready) {
        // Nothing this node measures can leave the board after this, so say so
        // loudly rather than transmitting into the void.
        Serial.println(F("[RadioTransmitter] init failed - no packets will be sent"));
        return;
    }
    Serial.print(F("[RadioTransmitter] ready on d"));
    Serial.print(PIN_RF433_TX);
    Serial.print(F(" at "));
    Serial.print(RF433_BITRATE);
    Serial.println(F(" bps"));
}

void RadioTransmitter::update(SystemState& state) {
    if (!_ready) return;

    uint32_t now = millis();
    if (now - _lastSendMs < INTERVAL_SEND_MS) return;
    _lastSendMs = now;

    SensorPacket packet;
    packet.version    = SENSOR_PROTOCOL_VERSION;
    packet.seq        = state.txSeq++;
    packet.distanceMm = state.distanceMm;
    packet.tempC_x10  = (int16_t)(state.tempC * 10.0f);

    // Faulted readings still go out, flagged. Silence is indistinguishable
    // from a dead node or a dead link, whereas a flagged packet lets the ESP32
    // tell "sensor broken" apart from "nothing is arriving" — it already has
    // tankStale for the latter.
    packet.flags = (uint8_t)((state.rangeValid ? 0 : SENSOR_FLAG_RANGE_FAULT)
                           | (state.tempValid  ? 0 : SENSOR_FLAG_TEMP_FAULT));

    packet.crc = sensorPacketCrc(packet);

    _driver.send((uint8_t*)&packet, sizeof(packet));
    _driver.waitPacketSent();   // ~35ms for 8 bytes at 2000 bps

    state.txCount++;
    state.lastTxMs = now;
}
