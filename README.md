# Smart Pump Controller

A water-pump controller for a rooftop tank. An ESP32-S3 in the control box drives
the pump relay, meters the mains it draws, and shows the lot on a small colour
TFT. A battery-free Arduino Nano sitting on the tank measures the water level and
sends it down over 433 MHz.

Two firmwares, one repository, one shared wire format.

> **Status: not yet deployed.** Most of the firmware is written and compiles;
> comparatively little of it has been watched working on hardware. See
> [Current state](#current-state) before trusting anything here.

---

## The two boards

| | `pump-controller/` | `water_tank/` |
|---|---|---|
| MCU | ESP32-S3-WROOM-1 N16R8 | Arduino Nano (ATmega328) |
| Job | pump relay, power metering, display, buttons | measure water level and temperature |
| Radio | RH_ASK receiver, GPIO16 | FS1000A transmitter, D4 |
| Display | ST7735S 1.8" 128×160 over FSPI | none |
| Sensors | ZMPT101B mains voltage, ACS712-30A current | AJSR04M ultrasonic, DS18B20 |
| Power | USB / 5 V supply in the control box | 5 V at the tank |

The link is **one-way**. The Nano transmits every 2 s and never listens; the
ESP32 receives and never replies. Nothing on the tank needs to be told anything.

### The wire format

`share/SensorPacket/SensorPacket.h` is compiled into both firmwares via
`lib_extra_dirs = ../share`, so the two sides cannot drift apart. Eight packed
bytes:

```c
struct SensorPacket {
    uint8_t  version;      // rejected outright if it does not match
    uint8_t  seq;          // wraps at 255, used for drop detection
    uint16_t distanceMm;   // sensor face to water, temperature-corrected
    int16_t  tempC_x10;    // 23.5C -> 235
    uint8_t  flags;        // range fault, temp fault
    uint8_t  crc;          // CRC-8/Maxim over bytes 0..5
};
```

A packet is accepted only if both the version and the CRC check out. A version
mismatch is rejected exactly like a bad checksum — an older node on the same
frequency must never be parsed with the wrong field offsets.

**Ultrasonic distance is temperature-corrected on the Nano**, not on the ESP32.
The speed of sound moves about 0.6 m/s per °C, which is a real error over a few
metres of tank, and the DS18B20 is up there anyway. If it faults, the node falls
back to 25 °C and raises a flag rather than silently reporting a wrong distance.

---

## How the firmware is arranged

Every module is a folder under `lib/`, exposes `begin()` and
`update(SystemState&)`, and **never calls another module**. Everything flows
through one `SystemState` struct:

```
RadioReceiver ──┐                              ┌──▶ PumpDriver ──▶ relay
InputManager ───┼──▶  SystemState  ──▶ SceneEngine
PowerMeter ─────┤          │                   └──▶ (safety rules)
PumpTimer ──────┘          ├──▶ DisplayUI ──▶ TFT
                           ├──▶ PowerStats ──▶ NVS
                           └──▶ ConfigStore ──▶ NVS
```

A module that senses something writes its own fields. A module that acts reads
them. Nothing reaches sideways. Adding a module is three edits in `main.cpp`:
declare it, `begin()` it in `setup()`, `update()` it in the `loop()` pipeline.

`SystemState` also carries per-consumer change detection, so `DisplayUI` and
`CloudClient` each track what they have already seen without interfering with one
another.

### The scheduler

`main.cpp` is a flat non-blocking loop. **There is no `delay()` anywhere outside
`setup()`** — that is rule 1 and it is not negotiable. Time-sensitive modules run
every pass; the display repaints on a 500 ms interval, or immediately when a
button is pressed so no press ever waits out the interval.

---

## Using it

Five buttons: LEFT, UP, SELECT, RIGHT, DOWN. No BACK button — LEFT is back, and
holding LEFT is the jump home.

```
   HOME  ──RIGHT──▶  CONFIG  ──SELECT──▶  setting editor
     ◀────LEFT────       ◀─────LEFT────
```

Two gestures worth memorising:

- **Hold SELECT on the home screen** — forces the pump on or off, or cancels a
  running timer.
- **Hold LEFT** — gets you home from anywhere. On the home screen itself, a
  3 s hold toggles bypass instead.

The home screen shows tank level and temperature, pump state, the pump timer, and
a rolling 24-hour power box (runtime, cycles, kWh, and either live volts/amps/watts
while the pump runs or the 24 h averages while it does not).

**Full key map for every screen: [`docs/ui_guide.md`](pump-controller/docs/ui_guide.md).**

### Calibrating the tank

No tape measure and no tank geometry — two measured distances instead, which
handles any mount height and any tank shape. Watch the serial log with the tank
full, then empty:

```
[RadioReceiver] seq=42 dist=812mm -> 47% temp=24.3C flags=0x00
```

Enter each reading on the config page under `Tank Full` / `Tank Empty`. It is in
metres and the log is in millimetres, so `812mm` is `0.812`. Settings persist to
NVS, so this survives a reboot and does not need a reflash.

---

## Building

[PlatformIO](https://platformio.org/). Run from the folder containing the
`platformio.ini` you want.

```bash
cd pump-controller && pio run                 # build the ESP32
cd pump-controller && pio run -t upload       # flash it
cd pump-controller && pio device monitor      # 115200 baud

cd water_tank && pio run -t upload            # flash the Nano
```

On the ESP32 use the **USB** port, not the COM port, for both upload and monitor.

More commands in [`docs/pio_commands.md`](pump-controller/docs/pio_commands.md).

---

## Documentation

| Document | What it covers |
|---|---|
| [`docs/wiringe_guide.md`](pump-controller/docs/wiringe_guide.md) | Every wire, every pin, the BC547 driver stages, the mains procedure, and the power-sensor calibration |
| [`docs/ui_guide.md`](pump-controller/docs/ui_guide.md) | What every button does on every screen |
| [`docs/pio_commands.md`](pump-controller/docs/pio_commands.md) | PlatformIO CLI reference |
| [`docs/custom_font_generator.md`](pump-controller/docs/custom_font_generator.md) | Regenerating the icon font (rarely needed — the bundled one covers U+F013–U+F1EB) |
| [`.claude/plans/`](pump-controller/.claude/plans/) | One design doc per feature, each with a `> Status:` header |
| [`.claude/plans/milestones.md`](pump-controller/.claude/plans/milestones.md) | The phase roadmap and what is actually done |

**Wiring and `config.h` are changed together, never separately.** The guide says
so at the top and it means it.

---

## Current state

| Phase | Built | Confirmed on hardware |
|---|---|---|
| 1 — Skeleton | ✅ | ✅ |
| 2 — Local inputs, display, settings | ✅ | ❌ |
| 3 — Radio link, tank level | ✅ both boards | ❌ never run end to end |
| 4 — Power metering + pump control | 🟡 metering only | 🟡 mains calibrated |
| 5 — Wi-Fi, MQTT, remote control | ❌ | — |
| 6 — OTA, scene editor, hardening | ❌ | — |

**The gap between those columns is the honest state of this project.** The pump
does not yet run itself: `SceneEngine` has no AUTO logic, and `PumpDriver`
enforces only the minimum-off-time, not the maximum runtime or the stale-tank
refusal. Everything currently reaching the relay is a manual press, a timer, or a
test.

[`milestones.md`](pump-controller/.claude/plans/milestones.md) breaks this down
per deliverable.

---

## Safety

Mains voltage is involved. Non-negotiable:

1. **Mains is the last thing connected and the first thing disconnected.** Never
   probe it with the board powered.
2. **The safety rules in `PumpDriver` are never bypassed** — not for testing, not
   for convenience. The `Bypass` setting on the config page suppresses the *AUTO
   level logic* only; it does not and must not reach the interlocks.
3. **Bench-test the relay click with no mains connected** before wiring anything
   live.
4. **Never commit `secrets.h`.** Wi-Fi and MQTT credentials stay local.

The relay module is 5 V-logic and a 3.3 V pin cannot release it, so GPIO18 drives
a BC547 low-side switch. That transistor inverts, which is why `RELAY_ACTIVE_LOW`
is `0` even though the module's IN terminal is active-low. Section 6 of the wiring
guide derives this in full — read it before touching the relay.

---

## Licence

[MIT](LICENSE).
