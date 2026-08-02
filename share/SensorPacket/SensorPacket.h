// share/SensorPacket/SensorPacket.h
#pragma once
#include <stdint.h>

// The wire format for the one-way 433 MHz ASK link between the Nano sensor
// node (water_tank) and the ESP32 (pump-controller). This header is shared by
// both firmwares through lib_extra_dirs = ../share, so the two sides can never
// drift apart. Keep it dependency-free — it has to compile on AVR and Xtensa.

#define SENSOR_PROTOCOL_VERSION   1

// ---- Flag bits in SensorPacket.flags -------------------------------------
#define SENSOR_FLAG_RANGE_FAULT   0x01   // no echo, or echo outside the valid window
#define SENSOR_FLAG_TEMP_FAULT    0x02   // DS18B20 unreadable; tempC_x10 is the fallback

// ---- Packet --------------------------------------------------------------
// Field order is deliberate: distanceMm lands at offset 2 and tempC_x10 at
// offset 4, so both 16-bit fields are naturally aligned and the layout comes
// out identical on AVR and Xtensa (both little-endian). The packed attribute
// and the static_assert below turn that from an assumption into a guarantee.

struct __attribute__((packed)) SensorPacket {
    uint8_t  version;      // SENSOR_PROTOCOL_VERSION — receiver drops anything else
    uint8_t  seq;          // increments per transmission, wraps at 255
    uint16_t distanceMm;   // sensor face to water surface, temperature-corrected
    int16_t  tempC_x10;    // 23.5 C -> 235
    uint8_t  flags;
    uint8_t  crc;          // CRC-8 over bytes 0..5
};

static_assert(sizeof(SensorPacket) == 8, "SensorPacket must be 8 bytes on the wire");

// ---- CRC-8 ---------------------------------------------------------------
// CRC-8/Maxim (poly 0x31, reflected to 0x8C). Table-less so it costs flash
// rather than the Nano's 2 KB of SRAM. Covers everything except the crc byte
// itself, so a corrupted version or flags byte is caught too.

static inline uint8_t sensorPacketCrc(const SensorPacket& packet) {
    const uint8_t* bytes = (const uint8_t*)&packet;
    uint8_t crc = 0;

    for (uint8_t i = 0; i < sizeof(SensorPacket) - 1; i++) {
        uint8_t value = bytes[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            uint8_t mix = (crc ^ value) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            value >>= 1;
        }
    }
    return crc;
}

// The receiver's single acceptance test. A version mismatch is rejected the
// same way a bad checksum is — an older node on the same frequency must not
// be parsed with the wrong field offsets.
static inline bool sensorPacketValid(const SensorPacket& packet) {
    return packet.version == SENSOR_PROTOCOL_VERSION
        && packet.crc == sensorPacketCrc(packet);
}
