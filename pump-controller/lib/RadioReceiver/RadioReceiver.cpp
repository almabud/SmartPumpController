#include "RadioReceiver.h"
#include <string.h>

void RadioReceiver::begin() {
    _ready = _driver.init();
    if (!_ready) {
        // Nothing about the tank can reach this board after this, so say it
        // loudly rather than looking like a permanently silent link.
        Serial.println("[RadioReceiver] init failed - no tank data will arrive");
        return;
    }
    Serial.printf("[RadioReceiver] ready on GPIO%d at %d bps\n",
                  PIN_RF433_RX, RF433_BITRATE);
}

void RadioReceiver::_applyPacket(SystemState& state, const SensorPacket& packet) {
    state.lastPacketTimeMs = millis();
    state.tankStale        = false;
    state.tankSeq          = packet.seq;

    // The node substitutes a fallback temperature when the DS18B20 is
    // unreadable and flags it, so the value is always usable — the flag is
    // what says how much to trust it.
    state.tankTempFault = (packet.flags & SENSOR_FLAG_TEMP_FAULT) != 0;
    state.tankTempC     = packet.tempC_x10 / 10.0f;

    state.tankSensorFault = (packet.flags & SENSOR_FLAG_RANGE_FAULT) != 0;

    if (!state.tankSensorFault) {
        // Larger distance = less water. The guard covers a mis-entered
        // calibration, which becomes reachable once the config page lands.
        if (state.tankEmptyMm > state.tankFullMm) {
            int32_t span = (int32_t)state.tankEmptyMm - (int32_t)state.tankFullMm;
            int32_t pct  = ((int32_t)state.tankEmptyMm - (int32_t)packet.distanceMm) * 100 / span;
            state.tankLevelPct = (uint8_t)constrain(pct, 0, 100);
        }
    }
    // A range fault leaves tankLevelPct alone: the last good reading stands,
    // marked suspect by tankSensorFault. Blanking it would make a broken
    // sensor indistinguishable from a dead link, which tankStale already means.

    Serial.printf("[RadioReceiver] seq=%u dist=%umm -> %u%% temp=%.1fC flags=0x%02X\n",
                  packet.seq, packet.distanceMm, state.tankLevelPct,
                  state.tankTempC, packet.flags);
}

// Three numbers that between them localise a dead link:
//   edges == 0        -> the RX pin never moves. Wiring, divider, or module
//                        power — the ESP32 is not seeing the receiver at all.
//   edges high, all
//   counters 0        -> RF noise is reaching the pin but nothing demodulates.
//                        Bitrate mismatch, or the transmitter is silent.
//   bad > 0, good 0   -> frames arriving and failing CRC. Weak signal, missing
//                        antenna, or interference.
//   good > 0, pkt > 0 -> the link works; SensorPacket itself is being rejected
//                        (protocol version mismatch between the two firmwares).
void RadioReceiver::_logLinkStats() {
    Serial.printf("[RadioReceiver] link stats - rxGood=%u rxBad=%u badPkt=%u pinEdges=%lu/%lu\n",
                  _driver.rxGood(), _driver.rxBad(), _badPackets,
                  (unsigned long)_pinEdges, (unsigned long)_pinSamples);

    // A pin that never moved is a wiring question, not a radio one, and its DC
    // level says which. Only measured when the line is already dead — switching
    // the pin to ADC mode would disturb a link that was actually working.
    if (_pinEdges == 0) {
        uint32_t mv = analogReadMilliVolts(PIN_RF433_RX);
        pinMode(PIN_RF433_RX, INPUT);   // hand it back to RH_ASK
        Serial.printf("[RadioReceiver]   pin flat at %lumv - "
                      "~0=no power/not connected, ~1600=divider reversed, "
                      "~3400=connected but silent\n", (unsigned long)mv);
    }

    _pinEdges   = 0;
    _pinSamples = 0;
}

void RadioReceiver::update(SystemState& state) {
    if (!_ready) return;

    // Sampled once per pass rather than in a timed burst — the loop runs far
    // faster than the 2000 bps signal, and a blocking sample window would
    // break the no-delay rule for a diagnostic.
    uint8_t level = digitalRead(PIN_RF433_RX);
    if (level != _lastPinLevel) {
        _lastPinLevel = level;
        _pinEdges++;
    }
    _pinSamples++;

    uint8_t buf[sizeof(SensorPacket)];
    uint8_t len = sizeof(buf);

    // Drain everything waiting — a slow loop pass can leave more than one.
    while (_driver.recv(buf, &len)) {
        if (len == sizeof(SensorPacket)) {
            SensorPacket packet;
            memcpy(&packet, buf, sizeof(packet));

            // sensorPacketValid() checks the protocol version as well as the
            // CRC. RH_ASK has already checked its own frame CRC by this point,
            // so a rejection here means the two firmwares disagree about the
            // format — worth reporting rather than dropping silently.
            if (sensorPacketValid(packet)) {
                _applyPacket(state, packet);
            } else {
                _badPackets++;
                Serial.printf("[RadioReceiver] packet rejected - version=%u crc=0x%02X expected=0x%02X\n",
                              packet.version, packet.crc, sensorPacketCrc(packet));
            }
        } else {
            _badPackets++;
            Serial.printf("[RadioReceiver] wrong length - got %u, expected %u\n",
                          len, (unsigned)sizeof(SensorPacket));
        }
        len = sizeof(buf);   // recv() overwrites len; reset before the next call
    }

    if (millis() - _lastStatsMs >= RADIO_STATS_INTERVAL_MS) {
        _lastStatsMs = millis();
        _logLinkStats();
    }

    // Checked every pass, packet or not.
    if (!state.tankStale && millis() - state.lastPacketTimeMs > TANK_STALE_TIMEOUT_MS) {
        state.tankStale = true;
        Serial.println("[RadioReceiver] link lost - tank data stale");
    }
}
