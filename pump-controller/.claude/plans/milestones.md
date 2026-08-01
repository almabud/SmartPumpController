# Development Milestones — Water Pump Controller v2

> Each phase ends with a specific, testable, demonstrable outcome.
> Do not start the next phase until the current one is confirmed working.
> No phase depends on future ones being implemented.

---

## Phase 1 — Skeleton

**Goal:** the real application architecture compiles, boots, and runs. Replace the
Adafruit test suite with the actual non-blocking scheduler and module stubs.

**Deliverables:**
- `main.cpp` — thin non-blocking scheduler (`millis()`-based, no `delay()`)
- All module stubs with empty `.cpp` files that compile cleanly
- `DisplayUI` shows a live home screen reading from `SystemState`
- `PumpDriver` initializes relay GPIO to safe OFF state
- `InputManager` configures all 4 button pins with `INPUT_PULLUP`

**Checkpoint — phase is done when:**
- `pio run` compiles with zero errors
- Serial monitor prints all module init messages on boot
- Display shows the home screen (all zeros, STALE, WiFi: NO — correct for now)
- Board stays running indefinitely without crashing or rebooting

---

## Phase 2 — Local inputs

**Goal:** the 4 buttons drive the display menu. A user can navigate screens without
any sensor data or network connectivity.

**Deliverables:**
- `InputManager` — debounce (30ms), short press events, long-press detection (800ms)
- `DisplayUI` — full menu/screen state machine:
  - Home screen (status at a glance)
  - Manual control screen (pump ON/OFF override)
  - Settings screen (thresholds, mode switch)
  - About screen (firmware version, uptime)
- Long-press BACK from any screen = return to home screen instantly
- Button press gives visual feedback on display (highlight / invert)

**Checkpoint — phase is done when:**
- All 4 buttons register correctly in serial monitor
- Navigation between all screens works without crashes
- Long-press BACK reliably returns to home from any depth
- Manual pump override request is written to `SystemState` (relay not yet wired to
  real mains — just the relay click confirms GPIO control works)

---

## Phase 3 — Sensor data (radio link)

**Goal:** the Nano reads tank level and temperature, transmits over 433 MHz, and the
ESP32 receives, validates, and displays live tank data. Both boards working together
end to end.

**Deliverables — Nano (`water-tank`):**
- `TankSensor` — AJSR04M distance reading with temperature-corrected speed of sound
- DS18B20 temperature reading (used for speed-of-sound correction, not displayed alone)
- `RadioTransmitter` — builds `SensorPacket` with checksum, transmits on interval
- Nano `main.cpp` — sense → pack → transmit loop, `SENSOR_SEND_INTERVAL_MS` cadence

**Deliverables — ESP32 (`pump-controller`):**
- `RadioReceiver` — RH_ASK receive, checksum validation, packet parsing
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

**Deliverables:**
- `PowerMeter` — ADC sampling for ZMPT101B (voltage) and ACS712 (current), RMS
  calculation, kWh accumulation, power fault detection
- `SceneEngine` — hysteresis-based pump decision logic:
  - AUTO mode: pump ON when `tankLevel < TANK_LEVEL_LOW_PCT`,
    pump OFF when `tankLevel > TANK_LEVEL_HIGH_PCT`
  - MANUAL mode: pump state follows button/request, scene logic paused
  - Refuses to act on stale tank data regardless of mode
- `PumpDriver` — relay control with mandatory safety rules:
  - `PUMP_MIN_OFF_MS` enforced between every OFF→ON transition
  - `PUMP_MAX_RUN_MS` enforced as maximum continuous runtime
  - Refuses ON if `state.tankStale == true`
  - Refuses ON if `state.powerFault == true`
  - Safety rules apply in BOTH AUTO and MANUAL mode — no bypass
- Display home screen shows live V / A / W / kWh readings
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

| Phase | Focus | Key checkpoint |
|-------|-------|----------------|
| 1 | Skeleton | Boots, display shows home screen |
| 2 | Local inputs | 4 buttons navigate full menu |
| 3 | Sensor data | Live tank level on display, STALE works |
| 4 | Power + pump | Pump runs in AUTO, safety rules enforced |
| 5 | Connectivity | App sees live data, remote commands work |
| 6 | Polish + OTA | OTA works, 72h run test, first deployment |

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

