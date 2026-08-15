# Power monitor — staged execution

> Status: **planned** — no stage started.
> Design, rationale and the decision log live in [`power-monitor.md`](./power-monitor.md).
> This file is the execution track only; it does not restate *why*.

Seven stages. **Every stage compiles clean, flashes, and is verifiable on its own** — no stage leaves
the tree in a state where the pump misbehaves or the screen breaks. Stages 1-3 are firmware-only and
invisible on screen; 4-5 add data; 6-7 add the UI.

The hard gate is **Stage 2**. Nothing downstream means anything until the sensors are calibrated, so
do not carry on past it on the strength of a clean build.

| # | Stage | Touches hardware? | Blocking risk |
|---|---|---|---|
| 1 | Pins + config constants | no | none — mechanical |
| 2 | PowerMeter: sampling → live V/A/W/Hz on serial | **yes — mains** | **gate: calibration** |
| 3 | PowerMeter: energy + powerFault | yes | fault thresholds vs. real grid |
| 4 | PowerStats: 24h ring in RAM | no | pipeline ordering |
| 5 | PowerStats: NVS persistence | no | flash wear, schema versioning |
| 6 | UI: stat row helper + the box | no | 112x62 px budget |
| 7 | UI: three-target focus walk | no | regressing the timer editor |

---

## Stage 1 — Pins and configuration

Mechanical. No runtime behavior changes; nothing reads these values yet.

**Files:** `include/config.h`

- Swap `:4-6` to `PIN_CURRENT_SENSE 4` / `PIN_VOLTAGE_SENSE 5`, and correct the divider comment from
  "10k/20k" to the actual 2.1k/4.6k.
- Add the `// ---------- Power metering ----------` and `// ---------- 24h stats ----------` blocks.
- Delete `INTERVAL_POWER_MS` at `:73` — reserved slot, referenced nowhere, superseded by
  `POWER_WINDOW_MS`.

Leave `BOOT_TOTAL_STEPS` at 4 for now — it gets bumped in the stage that actually adds a boot step.

**Verify:** `pio run` compiles clean. Flash and confirm the board behaves exactly as before — nothing
should change, and that is the whole point of the check.

**Done when:** build is clean and `grep -n "PIN_CURRENT_SENSE\|PIN_VOLTAGE_SENSE" include/config.h
docs/wiringe_guide.md` shows the header and the guide agreeing (4 = current, 5 = voltage).

**Commit:** `fix: correct swapped voltage/current sense pins in config`

---

## Stage 2 — PowerMeter sampling → live readings on serial

**The gate.** Ends with trusted volts, amps, watts and hertz printing once a second. No energy
totals, no fault flag, no UI.

**Files:** `lib/PowerMeter/PowerMeter.{h,cpp}`, `src/main.cpp`, `include/config.h`,
`docs/wiringe_guide.md`

- Fill in the stub: `micros()`-gated sampling at `ADC_SAMPLE_INTERVAL_US`, per-channel IIR DC-offset
  tracking, `_sumV2` / `_sumI2` / `_sumVI` accumulation, zero-crossing count, publish once per
  `POWER_WINDOW_MS` into `state.voltage` / `current` / `powerWatts` / `frequency`.
- `main.cpp`: include, instantiate, `powerMeter.begin()` with a boot step, and
  `powerMeter.update(state)` in the pipeline **after `radioReceiver.update()`, before
  `pumpTimer.update()`** — it must run every pass or the sample rate collapses.
- `BOOT_TOTAL_STEPS` 4 → 5.
- Add the `POWER_CAL_LOG` flag and its per-window dump.
- New "Calibrating the power sensors" section in `docs/wiringe_guide.md`, plus the new constants
  added to the embedded snippets at `:141-146` and `:164-167`.

**Verify, in order — do not skip 2a:**

**2a. Loop rate.** Log samples-per-window. Expect ~2000. If materially lower, `analogReadMilliVolts`
is too slow on this build: switch to `analogRead` + `analogSetPinAttenuation(pin, ADC_11db)` and drop
`ADC_SAMPLE_INTERVAL_US` to what the loop sustains. Anything ≥1 kHz still gives 20 samples per mains
cycle. **Every number below is meaningless until this passes.**

**2b. Sensors unpowered.** `voltage ≈ 0`, `current ≈ 0`, `frequency` suppressed. Confirms the
offset tracker settles and nothing reads phantom signal. Do this before mains goes near the board.

**2c. Mains present, no load.** Run the calibration procedure: measure real mains with a multimeter,
set `ZMPT_CAL_V_PER_V = actual_volts / logged_pin_volts`, reflash. Then `voltage` should sit within a
volt or two of the meter and `frequency` at ~50 Hz. Current still ~0.

**2d. Known resistive load** (kettle, heat gun — PF ≈ 1, so watts should equal V×A). Compare
`powerWatts` against a plug-in energy meter. Adjust `ACS712_MV_PER_AMP` if the part is off its
nominal 66 mV/A.

**2e. The pump.** `powerWatts` must land meaningfully **below** `voltage × current` — that is the
power factor showing up. If the two are equal on an inductive load, the `_sumVI` path is wrong; fix
it here, because Stage 3's kWh inherits the error permanently.

**Done when:** 2a-2e all pass and `ZMPT_CAL_V_PER_V` holds a measured value, not the placeholder.

**Commit:** `feat: sample mains voltage and current, report real power`

---

## Stage 3 — Energy accumulation and power fault

Short stage, but it depends on Stage 2 being correct — a mis-scaled watt figure integrates into a
permanently wrong kWh.

**Files:** `lib/PowerMeter/PowerMeter.cpp`

- `state.energyKwh += powerWatts * (POWER_WINDOW_MS / 3600000.0f) / 1000.0f` per window.
- `state.powerFault` latches after `POWER_FAULT_CONFIRM_N` consecutive windows with `Vrms` outside
  `[POWER_V_MIN, POWER_V_MAX]`; clears on one good window.
- Apply the `POWER_NOISE_FLOOR_A` clamp to `state.current` if it was not already added in Stage 2.

**Verify:**

- Run the known load from 2d for a measured 10 minutes; `energyKwh` should match
  `watts × hours / 1000` to within a percent or two.
- Pump idle: `current` reads exactly 0, not a drifting tenth of an amp. If it floats, raise
  `POWER_NOISE_FLOOR_A`.
- Fault path: with the sensors unpowered, `powerFault` latches true after 3 windows and clears one
  window after mains returns. Confirm it does **not** chatter on a normal grid — watch it idle for a
  few minutes and widen `POWER_V_MIN`/`POWER_V_MAX` if your supply sags past them in normal use.

**Done when:** kWh tracks a known load, idle current reads zero, and the fault flag is stable
overnight on the real grid without false latches.

**Note:** `powerFault` is *reported only*. It deliberately does not block the pump — see the
out-of-scope section in `power-monitor.md`.

**Commit:** `feat: accumulate energy and latch mains voltage faults`

---

## Stage 4 — PowerStats: the 24h ring, in RAM

No persistence yet — everything resets on reboot. Keeps the ring logic and the NVS work in separate,
separately-debuggable stages.

**Files:** `lib/PowerStats/PowerStats.{h,cpp}` (new), `lib/SystemState/SystemState.h`,
`src/main.cpp`, `include/config.h`

- New module with the standard `begin()` / `update(SystemState&)` contract.
- `HourBucket` struct, 24-entry ring, head index, `_hourAccumMs` accumulator, `_advanceBucket()`.
- Cycle counting via a private `PumpState _prevPumpState` — **not** a third `Consumer`.
- Per-second accumulation into the head bucket; aggregate into the five new `stats24h*` fields.
- `SystemState.h`: add the five fields after `:131`. No `StateSnapshot`, no `Field`, no
  `SystemState.cpp` changes.
- `main.cpp`: instantiate, `begin()` with a boot step, and `powerStats.update(state)` **last in the
  pipeline, after `pumpDriver.update()`** so it sees the pump state this pass settled on.
- `BOOT_TOTAL_STEPS` 5 → 6.

**Verify** — set `POWER_STATS_BUCKET_MS` to `60000` so an "hour" is a minute:

- Arm the timer with total `00:02`, break `00:01`, run `00:01` via the editor to force fast ON/OFF
  cycles. `Cyc` increments once per OFF→ON edge — not twice, and not on OFF.
- `Run` accumulates during RUN slices only, and stays frozen through BREAK slices.
- `kWh` climbs only while the pump is on.
- Avg current is the mean **while running** — cross-check it against the live watts/volts, not
  against a whole-day average.
- Leave it 24+ minutes: the ring rolls, and the oldest bucket drops out of the totals rather than
  accumulating forever.

**Restore `POWER_STATS_BUCKET_MS` to `3600000UL` before committing.**

**Done when:** all five stats track correctly through a full ring roll on the shortened period.

**Commit:** `feat: track rolling 24h pump runtime, cycles and energy`

---

## Stage 5 — NVS persistence

First NVS use in the project, so this stage sets the naming convention. Isolated deliberately: flash
wear and schema versioning are their own class of bug.

**Files:** `lib/PowerStats/PowerStats.cpp`

- Arduino `Preferences`, namespace `"pumpctl"`, keys `pwr.ver` / `pwr.buckets` / `pwr.head` /
  `pwr.accum` / `pwr.kwh`.
- `begin()` loads the ring, checks `pwr.ver`, restores `state.energyKwh`.
- Persist on every `_advanceBucket()`, and every `POWER_STATS_FLUSH_MS` — **only when the dirty flag
  is set.** The flag is set whenever the head bucket is modified, so idle hours cost zero writes.

**Verify:**

- Accumulate stats, then hard-reboot (power-cycle, not just reset). The box repopulates from flash;
  `energyKwh` survives.
- Bump `POWER_STATS_NVS_VER`, reboot: stats reset cleanly rather than reading garbage out of a
  stale blob.
- Leave the pump **off** for an hour and confirm no NVS writes occur — that is the dirty flag
  earning its keep. Log the write count to check this rather than inferring it.
- First boot on a device with no NVS namespace yet must not hang or crash.

**Done when:** stats survive a power cycle, a version bump resets cleanly, and an idle hour writes
nothing.

**Commit:** `feat: persist 24h power stats to NVS`

---

## Stage 6 — UI: the "Last 24h" box

Draws the box unfocusable, so the layout can be judged on its own before navigation changes land.

**Files:** `lib/DisplayUI/HomeUI.cpp`, `lib/DisplayUI/DisplayUI.h`

- New `_drawStatRow(boxX, boxW, y, label, value, valueColor)` helper — label hard left, value
  right-aligned via `_sprite.textWidth()`. The first genuinely reusable draw helper in the file.
- New `_drawPowerStats()` at `X=46, Y=52, W=112, H=62`; title strip in `0x2945` to match the title
  bar at `:61-62`; four rows at y=68/80/92/104, font 1, 12 px pitch.
- Bottom row contextual: pump ON → `"%.1fA  %.0fW"` in `TFT_GREEN`; pump OFF →
  `"avg %.1f  pk %.1f"` in `TFT_WHITE`.
- kWh row value turns `TFT_RED` when `state.powerFault`.
- Border `TFT_DARKGREY` for now — the focused case arrives in Stage 7.
- Call from `_drawHome()` after `_drawPumpTimer(state)` at `:282`.
- Delete the dead commented-out power block at `:304-331`.

**Verify:**

- Nothing clips or overlaps: the box must clear the tank column (x0-44, down to y126), the pump
  circle (x45-75, y18-48), the timer box (x80-159, y15-49), and the uptime line at `(100,118)`.
- Widest strings still fit 108 px of inner width — check `avg 12.4  pk 15.1` and a 5-digit watts
  value, not just the happy path.
- Run the pump and watch the bottom row flip green to live readings and back to avg/pk on stop.
- Force `powerFault` (unplug the voltage sensor) and confirm the kWh row goes red without shifting
  the layout.

**Done when:** the box renders correctly in both pump states and at fault, with no clipping.

**Commit:** `feat: add last-24h power stats box to the home screen`

---

## Stage 7 — UI: three-target focus walk

Last, and separate, because it edits shared navigation code that the timer editor depends on.

**Files:** `lib/DisplayUI/DisplayUI.h`, `lib/DisplayUI/DisplayUI.cpp`,
`lib/DisplayUI/HomeUI.cpp`, `.claude/plans/pump-timer.md`

- Append `POWER_STATS` to `FocusTarget` at `:15-18`.
- Replace the hardcoded two-state toggle at `DisplayUI.cpp:52-64` with the modular walk; DOWN goes
  forward, UP goes backward, LEFT still drops to `NONE`.
- `_drawPowerStats()` border becomes `TFT_YELLOW` when `_focus == FocusTarget::POWER_STATS`,
  mirroring the timer box at `HomeUI.cpp:193`.
- SELECT needs no change — `:66-69` already guards on `PUMP_TIMER`, so the new target falls through
  as the intended no-op. Confirm this by reading it rather than assuming.
- Update the key map at `pump-timer.md:41-56` with the new walk order.

**Verify — the regression check matters more than the new behavior:**

- DOWN walks NONE → timer → power → NONE; UP walks the reverse; LEFT drops to NONE from either.
- **The timer editor still works end to end**: SELECT on the focused timer box opens it, fields edit,
  SELECT commits, SELECT-long discards, and the committed timer actually runs the pump.
- SELECT on the power box does nothing at all — no editor, no crash, no stray redraw.
- `UI_FOCUS_TIMEOUT_MS` (10 s) still drops focus from **both** boxes.
- Long-press SELECT with no focus still toggles the pump (`InputManager.cpp:65-87`) — that path
  bypasses the UI and must be unaffected.

**Done when:** the walk is correct and every timer-editor behavior is unchanged.

**Commit:** `feat: make the 24h stats box focusable`

---

## Closing out

After Stage 7: update `.claude/plans/milestones.md` to mark the Phase 4 power-sensing items done, and
record that the `powerFault` → PumpDriver interlock is still outstanding. Flip the status header on
both `power-monitor.md` and this file to **done**, with the shipping commit range.

## If a stage has to be abandoned

Stages 1-3 are the only ones that must land together to be useful — a calibrated meter with no stats
is still a working meter. Stopping after Stage 3 leaves serial-only readings, which is a legitimate
place to pause. Stopping after Stage 5 leaves stats collected and persisted but invisible, which is
not — either finish Stage 6 or revert 4-5, rather than shipping a module that burns flash writes for
data nobody can see.
