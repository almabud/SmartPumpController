// water_tank/include/config.h
#pragma once

// ---------- Sensors ----------
#define PIN_SENSOR_TRIG        2    // AJSR04M TRIG
#define PIN_SENSOR_ECHO        3    // AJSR04M ECHO
#define PIN_TEMP_DATA          5    // DS18B20 1-Wire (external 4.6k pull-up to 5V)

// ---------- Radio: 433 ASK, transmit only ----------
#define PIN_RF433_TX           4    // module DATA pin; CS is tied to 5V in hardware
#define RF433_BITRATE       2000    // must match the ESP32 receiver

// RH_ASK's constructor claims an RX pin and a PTT pin whether we use them or
// not, and pinMode(255) reads out of bounds on AVR — so hand it real pins and
// keep them clear. Neither is wired to anything.
#define PIN_RF433_RX_UNUSED   11
#define PIN_RF433_PTT_UNUSED  10

// ---------- Ranging ----------
#define SENSOR_MIN_CM         20    // AJSR04M blind zone — closer reads are rejected
#define SENSOR_MAX_CM        450    // rated maximum
#define SENSOR_PING_SAMPLES    5    // median of N pings rejects surface-ripple spikes
#define TEMP_FALLBACK_C    25.0f    // speed-of-sound temperature when the DS18B20 is faulted

// ---------- Temperature ----------
#define TEMP_RESOLUTION_BITS  12    // 12-bit = 0.0625 C steps, 750 ms conversion
#define TEMP_CONVERSION_MS   750    // must match the resolution above
#define TEMP_RESET_VALUE_C 85.0f    // DS18B20 power-on value — a bad pull-up returns it

// ---------- Scheduler intervals ----------
#define INTERVAL_RANGE_MS   2000
#define INTERVAL_TEMP_MS   10000    // air temperature moves slowly
#define INTERVAL_SEND_MS    2000
#define INTERVAL_LOG_MS     5000

// ---------- Firmware ----------
#define FIRMWARE_VERSION  "v2.0"
