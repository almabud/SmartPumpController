# Development Milestones — Water Pump Controller v2

> Each phase ends with a specific, testable, demonstrable outcome.
> Do not start the next phase until the current one is confirmed working.
> No phase depends on future ones being implemented.

---

## Where things stand

Last updated at `d3df76e` on `feat/wifi`.

| Phase | Built | Confirmed on hardware |
|-------|-------|----------------------|
| 1 — Skeleton | ✅ | ✅ |
| 2 — Local inputs | ✅ except the About screen | ❌ nothing since the pump timer |
| 3 — Sensor data | ✅ both boards | ❌ never run against the live Nano |
| 4 — Power + pump | 🟡 sensing done, control logic not started | 🟡 mains calibration done |
| 5 — Connectivity | ❌ | — |
| 6 — Polish + OTA | ❌ | — |

**The gap between those two columns is the real state of this project.** Almost
everything compiles and almost nothing has been watched working. Every plan doc
under `.claude/plans/` carries its own `> Status:` header saying which of the two
it has reached; most read "implemented, builds clean, not confirmed on hardware".

The backlog of unverified work, oldest first: the boot screen, the manual pump
toggle, the pump timer, the radio link end to end, the config page, and the
bypass shortcut. A single session with the board and the Nano powered would close
most of it.

---

## Phase 1 — Skeleton

**Goal:** the real application architecture compiles, boots, and runs. Replace the
Adafruit test suite with the actual non-blocking scheduler and module stubs.

**Deliverables:**
- `main.cpp` — thin non-blocking scheduler (`millis()`-based, no `delay()`)
- All module stubs with empty `.cpp` files that compile cleanly
- `DisplayUI` shows a live home screen reading from `SystemState`
- `PumpDriver` initializes relay GPIO to safe OFF state
- `InputManager` configures all 5 button pins with `INPUT_PULLUP`

**Checkpoint — phase is done when:**
- `pio run` compiles with zero errors
- Serial monitor prints all module init messages on boot
- Display shows the home screen (all zeros, STALE, WiFi: NO — correct for now)
- Board stays running indefinitely without crashing or rebooting

---

## Phase 2 — Local inputs

**Goal:** the 5 buttons drive the display menu. A user can navigate screens without
any sensor data or network connectivity.

> **Progress.** Built, apart from the About screen. The full key map across every
> screen is documented in [`ui_guide.md`](../../docs/ui_guide.md) — that is the
> reference now, not this list. **None of it has been confirmed on hardware since
> the pump timer landed**, which means the config page, the bypass shortcut and
> the title-bar icon are all unwatched.
>
> The phase also grew past its original scope: settings now persist to NVS
> (`ConfigStore`), which Phase 6's scene editor was going to need anyway.

**Deliverables:**
- `InputManager` — debounce (30ms), short press events, long-press detection
  — *done, and since generalised: hold times are per-button, because LEFT needs
  two of them (2 s to go back a screen, 3 s to toggle bypass on home).*
- `DisplayUI` — full menu/screen state machine:
  - Home screen (status at a glance) — *done*
  - Manual control screen (pump ON/OFF override) — *not written, and not
    planned. The home screen's SELECT hold covers the override in one gesture,
    which is better than a screen you have to navigate to in an emergency.*
  - Settings screen — *done, as the config page: a table-driven list plus a
    per-setting editor, persisted to NVS (see [config-page.md](config-page.md)).*
  - About screen (firmware version, uptime) — *not written. Uptime is already on
    the home screen; the version only appears on the boot screen.*
- Long-press BACK from any screen = return to home screen instantly — *done as a
  2 s LEFT hold; there is no dedicated BACK button on this board*
- Button press gives visual feedback on display (highlight / invert)
  — *done: a yellow border on a focused home widget, a grey bar on the selected
  config row, and a blinking cursor on the field being edited*
- Pump timer on the home screen — a pump-on run budget plus an optional
  BREAK/RUN cycle, edited with the pad and driven by `PumpTimer`
  (see [pump-timer.md](pump-timer.md)) — *done*
- **Added, not originally scoped:** a bypass flag with a title-bar icon and a
  3 s LEFT-hold shortcut (see [bypass-shortcut.md](bypass-shortcut.md)), and
  `ConfigStore` for NVS-backed settings

**Checkpoint — phase is done when:**
- ~~All 5 buttons register correctly in serial monitor~~ — done
- Navigation between all screens works without crashes — **outstanding**
- Long-press BACK reliably returns to home from any depth — **outstanding**
- ~~Manual pump override request is written to `SystemState`~~ — done in code;
  the relay click itself is **outstanding**

---

## Phase 3 — Sensor data (radio link)

**Goal:** the Nano reads tank level and temperature, transmits over 433 MHz, and the
ESP32 receives, validates, and displays live tank data. Both boards working together
end to end.

> **Progress.** Both boards are built. The Nano was re-architected to match the
> pump-controller's module design in `b9e9238`, and `RadioReceiver` landed with
> tank level, staleness and temperature. **The two have never been run against
> each other** — see [radio-receiver.md](radio-receiver.md), whose status is
> "not yet exercised against the live Nano". Until that happens the whole phase
> is unconfirmed, and Phase 4's AUTO logic has nothing trustworthy to act on.

**Deliverables — Nano (`water-tank`):**
- `TankSensor` — AJSR04M distance reading with temperature-corrected speed of sound
- DS18B20 temperature reading (used for speed-of-sound correction, not displayed alone)
- `RadioTransmitter` — builds `SensorPacket` with checksum, transmits on interval
- Nano `main.cpp` — sense → pack → transmit loop, `SENSOR_SEND_INTERVAL_MS` cadence

**Deliverables — ESP32 (`pump-controller`):**
- `RadioReceiver` — RH_ASK receive, checksum validation, packet parsing
  (implemented — see [radio-receiver.md](radio-receiver.md))
- Staleness detection — if no valid packet for `TANK_STALE_TIMEOUT_MS`, set
  `state.tankStale = true` and stop using the reading for pump decisions
- Tank level % calculated from raw distance + tank dimensions in `config.h`
- Home screen shows live tank level updating in real time
- STALE flag appears on display when radio link is lost

**Checkpoint — phase is done when:**
- Serial monitor on ESP32 shows valid packets arriving from Nano
- Tank level % updates live on the display
- Physically blocking or unplugging the Nano triggers the STALE flag within
  `TANK_STALE_TIMEOUT_MS` milliseconds
- Restoring the Nano clears the STALE flag

---

## Phase 4 — Power sensing + pump control

**Goal:** the system reads real power data, makes pump decisions based on tank level,
and drives the relay safely. The core control loop is fully operational.

> **Progress.** The sensing half is built and now calibrated against real mains
> (`6018921`) — see [`power-monitor.md`](./power-monitor.md) and
> [`power-monitor-steps.md`](./power-monitor-steps.md). Shipped: ADC sampling and
> RMS, the phase correction between the two channels, a rolling 24h stats window
> persisted to NVS, and a focusable stats box on the home screen.
>
> **The control half has not been started.** `SceneEngine` still consumes a
> manual request and returns; there is no AUTO logic, and
> `TANK_LEVEL_LOW_PCT` / `TANK_LEVEL_HIGH_PCT` do not exist in `config.h` yet.
> `PumpDriver` enforces `PUMP_MIN_OFF_MS` and nothing else — no max runtime, no
> stale-tank refusal, no power-fault refusal. Lifetime `state.energyKwh` is
> persisted but never written, and `powerFault` is never set.
>
> The bypass flag from Phase 2 is already wired into `SceneEngine` as a guard
> ahead of the AUTO block, so the switch exists before the thing it switches off.
> Whoever writes the AUTO logic must write it below that guard.

**Deliverables:**
- `PowerMeter` — ADC sampling for ZMPT101B (voltage) and ACS712 (current), RMS
  calculation, kWh accumulation, power fault detection
  — *sampling, RMS and mains calibration done; lifetime kWh and fault detection
  outstanding. Power is computed as `mean(v*i)` rather than `Vrms*Irms`, since a
  pump motor's power factor of ~0.7-0.85 would otherwise overstate every figure.*
- `SceneEngine` — hysteresis-based pump decision logic:
  - AUTO mode: pump ON when `tankLevel < TANK_LEVEL_LOW_PCT`,
    pump OFF when `tankLevel > TANK_LEVEL_HIGH_PCT`
  - MANUAL mode: pump state follows button/request, scene logic paused
  - Refuses to act on stale tank data regardless of mode
- `PumpDriver` — relay control with mandatory safety rules:
  - `PUMP_MIN_OFF_MS` enforced between every OFF→ON transition — *done*
  - `PUMP_MAX_RUN_MS` enforced as maximum continuous runtime — *outstanding; the
    constant is not in `config.h` yet*
  - Refuses ON if `state.tankStale == true` — *outstanding*
  - Refuses ON if `state.powerFault == true`
    — *deliberately deferred. Shipping this alongside brand-new, uncalibrated
    sensor math means one bad voltage reading refuses to run the pump. It lands
    once the readings are trusted on hardware.*
  - Safety rules apply in BOTH AUTO and MANUAL mode — no bypass
- Display home screen shows live V / A / W / kWh readings
  — *done, as the "Last 24h" box: runtime, cycles and kWh, with the bottom row
  swapping to live amps/watts while the pump runs.*
- Manual override tested and confirmed: buttons can force pump on/off,
  safety rules still apply

**Checkpoint — phase is done when:**
- V / A / W / kWh read correctly and display on home screen
- In AUTO mode: pump clicks on when tank drops below LOW threshold,
  clicks off when tank rises above HIGH threshold
- Manually unplugging the Nano (tank stale) prevents the pump from running in AUTO
- `PUMP_MIN_OFF_MS` is demonstrably enforced (pump cannot be rapid-cycled)
- Manual override works from the buttons menu

> ⚠️ Mains wiring for this phase: connect ZMPT101B and ACS712 to mains only after
> all low-voltage (signal-side) testing is complete. Never probe mains with the
> board powered. Test relay click first on the bench with no mains connected.

---

## Phase 5 — Connectivity

**Goal:** the device connects to Wi-Fi, publishes status to an MQTT broker, and
accepts remote commands from the app. Remote commands go through the same safety
checks as local ones.

**Deliverables:**
- **BT provisioning** — Bluetooth-based Wi-Fi credential setup so credentials are
  never hardcoded in firmware. User pairs phone to device, sends SSID + password,
  device stores in NVS (non-volatile storage), connects automatically on boot.
- `WifiManager` — non-blocking connect/reconnect with exponential backoff.
  Connectivity loss is a normal state — never blocks the control loop.
- `CloudClient` — MQTT client (PubSubClient):
  - Publishes status on `devices/{deviceId}/status` (interval + on change):
    pump state, tank level, V/A/W/kWh, mode, fault flags, connectivity
  - Subscribes to `devices/{deviceId}/command`
  - Incoming command → written to `SystemState` as `ActionRequest` —
    never calls a driver directly
  - Per-device credentials (not a shared global secret)
- Display home screen shows WiFi and MQTT connection status
- `StatusLED` — onboard RGB (GPIO48) shows connectivity and fault state:
  - Green = healthy, AUTO, connected
  - Amber = offline / local-only
  - Blue = pump running
  - Red (blinking) = fault

**Checkpoint — phase is done when:**
- Device connects to Wi-Fi via BT provisioning (no hardcoded credentials)
- MQTT broker receives live status updates
- App (or MQTT client tool like MQTT Explorer) can send a pump-on command
  and the relay responds correctly
- Sending a pump-on command when tank is STALE is correctly refused
- Disconnecting Wi-Fi mid-operation does not affect local pump control

---

## Phase 6 — Polish + OTA

**Goal:** the device is production-ready. OTA firmware updates work, the scene
editor is usable, fault handling is robust, and the first house deployment happens.

**Deliverables:**
- **OTA firmware update** — device polls server for new firmware on boot and
  periodically. Downloads and applies update to the inactive OTA partition.
  Rolls back automatically if the new firmware fails to boot.
- **Scene editor UI** — display menu lets user edit scene parameters
  (LOW threshold, HIGH threshold, max runtime) saved to NVS
  — *most of the machinery arrived early with the config page. The list is
  table-driven and `ConfigStore` already persists to NVS, so each threshold is
  one `ConfigItem` member plus one `CONFIG_DEFS` row. What is missing is the
  scene concept itself, and a percent/duration editor kind alongside the
  existing metres and on/off ones.*
- **Fault handling** — explicit fault screens with clear user-readable messages,
  fault logging to NVS for later retrieval, MQTT fault alerts to app
- **Production hardening**:
  - Watchdog timer enabled (device auto-recovers from hangs)
  - All tunables confirmed and locked in `config.h`
  - Mains wiring fully inspected, fused, and enclosed
  - Device runs unattended for 72 hours without issue

**Checkpoint — phase is done when:**
- OTA update successfully delivered and applied over the air
- Scene parameters editable on the display and persisted across reboots
- 72-hour unattended run test passes (no crashes, no spurious relay trips,
  no memory leaks visible in uptime/heap monitoring)
- First house deployment complete and monitored for one week

---

## Summary table

| Phase | Focus | Key checkpoint | State |
|-------|-------|----------------|-------|
| 1 | Skeleton | Boots, display shows home screen | ✅ done |
| 2 | Local inputs | 5 buttons navigate full menu | 🟡 built, unverified |
| 3 | Sensor data | Live tank level on display, STALE works | 🟡 built, never run end to end |
| 4 | Power + pump | Pump runs in AUTO, safety rules enforced | 🟡 sensing done, control not started |
| 5 | Connectivity | App sees live data, remote commands work | ❌ not started |
| 6 | Polish + OTA | OTA works, 72h run test, first deployment | ❌ not started |

---

## Rules for all phases

1. **No `delay()` anywhere in the ESP32 firmware.** Ever.
2. **Each phase is testable independently** — do not skip ahead.
3. **Safety rules in `PumpDriver` are never bypassed** — not for testing, not for
   convenience. If you need to test without the safety rules, comment them out
   explicitly and restore them before the phase checkpoint.
4. **Never commit `secrets.h`** — Wi-Fi/MQTT credentials stay local only.
5. **Mains wiring is always the last thing connected** and the first thing
   disconnected during any rework. Never probe mains while the board is powered.

