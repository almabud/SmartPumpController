# RadioReceiver — 433 MHz ASK link from the tank node

> Status: **implemented, builds clean**. Not yet exercised against the live Nano;
> the calibration values in `config.h` are placeholders until measured on site.

## Context

`RadioReceiver` was a stub that printed `begin (stub - Phase 3)` and was never
instantiated in `main.cpp`. The home screen's tank column was drawing hardcoded
defaults — `tankLevelPct = 50`, `tankTempC = 27.5f`, `tankStale = false` — which
looked exactly like live data. This closes the link end to end and is Phase 3 of
[milestones.md](milestones.md).

## The wire format is shared, not reimplemented

`share/SensorPacket/SensorPacket.h` is included by **both** firmwares through
`lib_extra_dirs = ../share`, so the two ends cannot drift. It is dependency-free
so it compiles on AVR and Xtensa alike.

```cpp
struct __attribute__((packed)) SensorPacket {   // 8 bytes, static_assert'd
    uint8_t  version;      // SENSOR_PROTOCOL_VERSION (1)
    uint8_t  seq;          // increments per transmission, wraps at 255
    uint16_t distanceMm;   // sensor face to water surface, temperature-corrected
    int16_t  tempC_x10;    // 23.5 C -> 235
    uint8_t  flags;        // SENSOR_FLAG_RANGE_FAULT | SENSOR_FLAG_TEMP_FAULT
    uint8_t  crc;          // CRC-8/Maxim over bytes 0..5
};
```

Acceptance is `sensorPacketValid()` from that header — **do not hand-roll a
checksum here**. It gates on the protocol version as well as the CRC, on purpose:
an older node on the same frequency must be rejected rather than parsed at the
wrong field offsets.

`RF433_BITRATE` is 2000 on both sides. The node sends every 2 s
(`water_tank/include/config.h`).

## Two fault states, deliberately distinct

`RadioTransmitter.cpp:33` sends faulted readings **flagged rather than
suppressed**, because silence is indistinguishable from a dead node or a dead
link. Honouring that split is the whole reason `tankSensorFault` exists:

| Situation | `tankStale` | `tankSensorFault` | Level shown |
|---|---|---|---|
| Healthy | false | false | live |
| No echo / out of range | false | **true** | last good value, held |
| Node or link dead | **true** | unchanged | blanked by the display |

A range fault leaves `tankLevelPct` untouched. Blanking it would make a broken
sensor look identical to a dead link, which is precisely the distinction the
transmitter goes out of its way to send.

`tankTempFault` works the same way but the value is always stored — the node
substitutes its own fallback temperature and the flag says how much to trust it.

## Calibration

Two distances, measured rather than calculated: what the sensor reads with the
tank **full**, and what it reads **empty**. No tape measure, no arithmetic, and it
handles any mount height or tank shape.

```c
#define TANK_DISTANCE_FULL_MM    300   // TODO reading when the tank is FULL
#define TANK_DISTANCE_EMPTY_MM  1500   // TODO reading when the tank is EMPTY
```

Read both off the log line printed for every accepted packet — that line is a
deliverable, not debug scaffolding:

```
[RadioReceiver] seq=42 dist=812mm -> 47% temp=24.3C flags=0x00
```

```cpp
int32_t span = tankEmptyMm - tankFullMm;
int32_t pct  = (tankEmptyMm - distanceMm) * 100 / span;
tankLevelPct = constrain(pct, 0, 100);
```

Clamping means a distance past either calibration point reads as a clean 0% or
100% instead of wrapping. A `tankEmptyMm <= tankFullMm` guard skips the update
entirely — that becomes reachable once the values are user-editable.

**The values live in `SystemState`, seeded from `config.h`**, not read from the
macros at the point of conversion. The config page (next plan) writes
`state.tankFullMm` / `state.tankEmptyMm` and nothing in this module changes.

## Staleness

`TANK_STALE_TIMEOUT_MS` is 10 s — five missed packets at the node's 2 s interval.
Tolerates a noisy 433 link without letting stale data stand for long. Checked on
every `update()`, packet or not; the next valid packet clears it.

## SystemState defaults were wrong

The old tank defaults rendered as a plausible half-full tank at 27.5 °C before a
single packet had arrived. They now read as "nothing known": `tankLevelPct = 0`,
`tankTempC = 0.0f`, and `tankStale = true` until the first packet lands.

`tankSensorFault` and `tankTempFault` joined `StateSnapshot`, the `hasChanged()`
chain and `markSeen()`, plus `Field::TANK_SENSOR_FAULT` for field-specific checks.

## Pins

`RH_ASK`'s constructor claims a TX and a PTT pin whether or not this node ever
transmits, and drives them as outputs. GPIO7 and GPIO10 — the board's last two
spares — absorb that. **Nothing is wired to them, but they are no longer free.**
The library's own ESP32 example uses GPIO0 for this, which is a strapping pin on
the S3 and must not be driven.

`recv()` is not rate-limited. `INTERVAL_RADIO_MS` exists in `config.h` but stays
unused: `recv()` is a cheap non-blocking check of a flag the ISR sets, and gating
it would only add latency and risk the RH_ASK buffer being overwritten before it
is drained. `update()` drains in a `while` loop for the same reason.

## Not in scope

The **config page** — calibration stays compile-time defaults for now; making it
user-editable is the next plan.

The dead v1 files in the project root (`control_box.*`, `display.*`,
`water_tank_data_receiver.h`, `water_tank_data_reveiver.cpp`) contain an older
AES-based receiver. They are not compiled (PlatformIO builds `src/` only) and were
not used as a reference. Deleting them is a separate cleanup.

## Verification

1. `pio run` — compiles clean, dependency graph shows `RadioHead @ 1.120.0`
   (same version as the Nano). **Done.**
2. Flash both boards, serial monitor on each.
3. **Link up.** A `[RadioReceiver] seq=N ...` line about every 2 s with `seq`
   incrementing by exactly 1. A steady gap means the antenna (17.3 cm wire,
   mandatory) or the divider is wrong.
4. **Level tracks reality.** Move a board toward/away from the sensor: `dist`
   changes, `%` moves the opposite way, the tank bar redraws within 500 ms and
   changes colour at 50% and 20% via the existing `_drawTankLevel()`.
5. **Calibrate.** Note `dist` at full and empty, put them in `config.h`, reflash,
   confirm the extremes read exactly 100% and 0%.
6. **Stale.** Power the Nano down. Within 10 s: `link lost` logged, level and
   temperature blank, heartbeat dot red (`_drawHeartbeat` already keys off
   `tankStale`). Power up — the next packet clears it.
7. **Sensor fault vs link loss.** Hold something inside the sensor's 20 cm blind
   zone so the node sends `SENSOR_FLAG_RANGE_FAULT`. Packets keep arriving, so
   `tankStale` must stay **false**, `flags=0x01` appears in the log, and the level
   holds its last value instead of blanking. This is the check that proves the two
   fault states are actually distinct.
8. **Boot.** The boot bar still fills to 100% with the extra radio step, and an
   init failure is reported rather than passing silently.
