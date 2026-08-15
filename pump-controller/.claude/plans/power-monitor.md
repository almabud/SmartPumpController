# Power monitor — sensing, 24h rolling stats, home UI box

> Status: **planned** — not started.
> Execution is broken into 7 staged steps in
> [`power-monitor-steps.md`](./power-monitor-steps.md). This file holds the design and the
> decision log; that one holds the order of work and the per-stage verification.

## Context

The power side of the controller is empty scaffolding. `lib/PowerMeter/PowerMeter.cpp:3-9` is a
Phase-4 stub that prints a banner and does nothing, and it is not instantiated in `src/main.cpp` at
all. Meanwhile the hardware is already specified and the state layer is already built for it:
`SystemState.h:125-131` carries `voltage`, `current`, `powerWatts`, `energyKwh`, `frequency` and
`powerFault`, all six fully wired into change detection (`Field` at `:56-61`, `StateSnapshot` at
`:82-87`, and all three functions in `SystemState.cpp`). Nothing writes any of them, so the home
screen's power readout sits commented out at `HomeUI.cpp:304-331`.

This change makes the controller measure what the pump actually draws, and surfaces it on the home
screen as a bordered, focusable "Last 24h" box: total runtime, cycle count, energy consumed, and
average/peak current — with the bottom row switching to a live amps/watts readout while the pump is
running. The box is focusable from the outset so the DOWN-key walk order is settled now; SELECT on
it is a deliberate no-op until the server-backed 30-day view exists.

Two secondary outcomes: this establishes the project's first NVS persistence convention (there is
none anywhere in the codebase today), and it adds the reusable label/value row helper the drawing
code has been missing.

## Decisions

- **Pin truth.** The current sensor is physically on GPIO4 and the voltage sensor on GPIO5.
  `docs/wiringe_guide.md:127,143,158,166,370-371` is correct; **`config.h:5-6` has them swapped and
  gets fixed.** The docs version is also the self-consistent one — the ACS712 is the 5V part whose
  output reaches ~4.5V and therefore needs the 2.1k/4.6k divider, and `config.h:6` already carries a
  divider comment on the *current* line, so the intent was right and only the pin number was wrong.
  (That comment's "10k/20k" matches neither the board nor the guide and is corrected to 2.1k/4.6k.)
- **Real power, not apparent power.** Compute `mean(v[n] * i[n])`, not `Vrms * Irms`. A pump motor
  runs at PF ≈ 0.7-0.85; the apparent-power shortcut would overstate consumption by 15-30% and
  poison every kWh figure downstream. Both channels are sampled in the same pass tens of
  microseconds apart — well under a degree of phase error at 50 Hz — so the instantaneous product is
  valid.
- **Time base: `millis()` for now.** There is no RTC and no NTP. Hourly buckets advance on a
  `millis()` accumulator and persist to NVS; they re-anchor to real hours when NTP lands in Phase 5.
  The roll trigger is isolated in one function so that swap is a few lines.
- **Long history lives on the server.** The device keeps 24h locally and will push rollups. No
  on-device 30-day storage.
- **Box metrics**: runtime, cycles, kWh, avg+peak current. No cost line — the tariff is not a
  constant, it steps with consumption.
- **Bottom row is contextual**: pump ON → live `4.3A 940W` in green; pump OFF → `avg 4.2 pk 5.1`.
  Live numbers only matter while running; avg/peak is what you study afterwards. Neither row is
  wasted.
- **SELECT on the box does nothing** for now. Focus only.

## Safety

The ZMPT101B AC terminals and the ACS712 IP+/IP- pads carry live mains. Follow the existing sequence
in `docs/wiringe_guide.md:346-360`: bring up and calibrate on the bench first, wire mains last, and
do not probe the sensor board while the ESP32 is USB-connected to a non-isolated laptop.

---

## Changes

### `include/config.h`

Fix the swapped pins at `:4-6`:

```c
// ---------- Analog sensors (ADC1 only — Wi-Fi always on) ----------
#define PIN_CURRENT_SENSE   4    // ACS712-30A, 5V part, via 2.1k/4.6k divider
#define PIN_VOLTAGE_SENSE   5    // ZMPT101B (3.3V supply, pot-tuned, no divider)
```

Add near the existing calibration block at `:55-57` (which is kept as-is):

```c
// ---------- Power metering ----------
#define ADC_SAMPLE_INTERVAL_US    500     // ~2 kHz per channel; 40 samples per 50 Hz cycle
#define POWER_WINDOW_MS          1000     // one reported reading per window

// ZMPT101B has a trim pot, so this MUST be measured on the actual board — see
// the calibration procedure in docs/wiringe_guide.md. Mains volts per volt at GPIO5.
#define ZMPT_CAL_V_PER_V        220.0f    // PLACEHOLDER — calibrate before trusting any reading

#define POWER_NOISE_FLOOR_A       0.15f   // below this the ACS712 is reporting its own noise
#define POWER_V_MIN             180.0f    // sustained under this -> powerFault
#define POWER_V_MAX             260.0f    // sustained over this  -> powerFault
#define POWER_FAULT_CONFIRM_N        3    // consecutive bad windows before the flag latches

// ---------- 24h stats ----------
// Bucket period. Drop to 60000 to make an "hour" one minute and roll the whole
// 24-slot ring in 24 minutes during testing.
#define POWER_STATS_BUCKET_MS  3600000UL
#define POWER_STATS_BUCKETS         24
#define POWER_STATS_FLUSH_MS    600000UL  // flush the live bucket to NVS every 10 min, if dirty
#define POWER_STATS_NVS_VER          1    // bump to discard buckets after a struct change
```

Also: `BOOT_TOTAL_STEPS` at `:102` goes 4 → 6 (the comment there already says "bump as modules
land"), and `INTERVAL_POWER_MS` at `:73` is deleted — it is the reserved slot for this feature,
referenced nowhere, and `POWER_WINDOW_MS` supersedes it. Two names for one thing is worse than one.

### `lib/PowerMeter/PowerMeter.{h,cpp}`

Fill in the stub, keeping the established `begin()` / `update(SystemState&)` contract
(`PowerMeter.h:5-9`). Called every loop pass; self-gates on `micros()`.

**No blocking bursts.** `milestones.md:202` forbids `delay()` in `loop()`, and a 40 ms RMS burst once
a second would visibly stutter the display. Use the pattern `RadioReceiver.cpp:80-88` already
establishes — one sample pair per eligible pass, accumulated across passes.

```
update(state):
  now = micros()
  if (now - _lastSampleUs >= ADC_SAMPLE_INTERVAL_US):
      _lastSampleUs += ADC_SAMPLE_INTERVAL_US        // fixed step, no drift
      vmv = analogReadMilliVolts(PIN_VOLTAGE_SENSE)
      imv = analogReadMilliVolts(PIN_CURRENT_SENSE)

      // Track each channel's DC bias with a slow IIR rather than assuming the
      // midpoint. The ACS712 sits at 2.5V * 0.686 = 1.715V and the ZMPT wherever
      // its pot puts it, and both drift with temperature and supply.
      _vOffset += (vmv - _vOffset) / 1024.0f
      _iOffset += (imv - _iOffset) / 1024.0f
      acV = vmv - _vOffset
      acI = imv - _iOffset

      _sumV2 += acV*acV;  _sumI2 += acI*acI;  _sumVI += acV*acI;  _n++
      if (acV crossed zero this sample) _crossings++

  if (millis() - _lastWindowMs >= POWER_WINDOW_MS):
      publish(state); reset accumulators
```

Publishing, per window:

- `Vrms_pin = sqrt(_sumV2/_n) / 1000.0f` → `state.voltage = Vrms_pin * ZMPT_CAL_V_PER_V`
- `Irms_pin = sqrt(_sumI2/_n) / 1000.0f` →
  `state.current = Irms_pin / ((ACS712_MV_PER_AMP/1000.0f) * ACS712_DIVIDER_RATIO)`,
  clamped to 0 below `POWER_NOISE_FLOOR_A`
- `state.powerWatts = (_sumVI/_n) * <both scale factors>`
- `state.frequency = _crossings / 2.0f / (POWER_WINDOW_MS/1000.0f)`, computed only when `Vrms` is
  above `POWER_V_MIN` — below that, noise produces nonsense crossings
- `state.energyKwh += state.powerWatts * (POWER_WINDOW_MS / 3600000.0f) / 1000.0f`
- `state.powerFault` latches true after `POWER_FAULT_CONFIRM_N` consecutive windows with `Vrms`
  outside `[POWER_V_MIN, POWER_V_MAX]`; clears on one good window

**Use `analogReadMilliVolts()`, not `analogRead()`.** It applies the chip's eFuse ADC calibration,
which removes the ESP32's well-documented ADC nonlinearity and the need to guess a reference
voltage. The v1 code at `control_box.cpp:331-339` used `raw * 5.0 / 1023.0` AVR math that does not
transfer to the S3 at all. `analogReadMilliVolts` is slower; if the loop cannot sustain 2 kHz (see
Verification), fall back to `analogRead()` with `analogSetPinAttenuation(pin, ADC_11db)` plus a
linear mV conversion and lower `ADC_SAMPLE_INTERVAL_US` to what the loop actually sustains —
anything at or above ~1 kHz still gives 20 samples per mains cycle.

`begin()` sets pin attenuation and logs with the established `[PowerMeter]` tag.

**Calibration log**: behind a `POWER_CAL_LOG` flag, print `Vrms_pin_mV`, `Irms_pin_mV` and the
derived values each window.

### `lib/PowerStats/PowerStats.{h,cpp}` — new module

Same contract as every other module. No cross-module calls — `SystemState` only.

```cpp
struct HourBucket {                 // 20 bytes; 24 of them = 480 B
    uint32_t energyMilliWh;
    uint32_t runtimeSec;
    uint32_t currentSumMa;          // summed once per pump-on second
    uint16_t currentSamples;        // count of those seconds, for a true mean
    uint16_t cycles;                // OFF->ON transitions
    uint16_t peakCurrentMa;
};
```

**Cycle counting**: keep a private `PumpState _prevPumpState` and count OFF→ON edges. Do *not* add a
third `Consumer` for this — `Consumer::_COUNT` is 2 (`SystemState.h:40-44`) and a new consumer means
touching `StateSnapshot` plus all three functions in `SystemState.cpp`. A local previous-value
compare is equivalent and zero-risk.

**Bucket advance**, isolated in one function so the Phase-5 NTP swap stays local:

```cpp
_hourAccumMs += (now - _lastTickMs);
while (_hourAccumMs >= POWER_STATS_BUCKET_MS) {
    _hourAccumMs -= POWER_STATS_BUCKET_MS;
    _advanceBucket();               // head++, zero the new head, persist, recompute totals
}
// Phase 5: replace the accumulator with `hour = epoch/3600` and roll when it changes.
```

Aggregation into `SystemState`, on every roll and every second:

- `stats24hRuntimeSec` = sum of `runtimeSec`
- `stats24hCycles` = sum of `cycles`
- `stats24hEnergyKwh` = sum of `energyMilliWh` / 1e6
- `stats24hAvgCurrent` = `sum(currentSumMa) / sum(currentSamples)` — **mean while running**, not mean
  across the whole day. Averaging in the off-hours just divides by 24 and buries the signal; the
  useful fact is that a healthy pump's running current is flat over weeks, and a creeping average
  means a worn impeller, a failing start capacitor, or a bearing going.
- `stats24hPeakCurrent` = max of `peakCurrentMa`

**NVS persistence** — first use in this project, so this sets the convention. Arduino `Preferences`,
namespace `"pumpctl"` (must be ≤15 chars), keys prefixed by owning module. The stock
`default_16MB.csv` partition table already includes an `nvs` partition; no partition change needed.

| Key | Type | Contents |
|---|---|---|
| `pwr.ver` | `uint8` | schema version; mismatch → discard and start clean |
| `pwr.buckets` | blob | the 24-entry `HourBucket` ring |
| `pwr.head` | `uint8` | head index |
| `pwr.accum` | `uint32` | ms elapsed into the current bucket |
| `pwr.kwh` | `float` | lifetime energy, restores `state.energyKwh` across reboot |

**Flash wear**: write on every bucket roll (24/day) and every `POWER_STATS_FLUSH_MS` — but **only if
a dirty flag is set**. The flag is set whenever the head bucket is modified, which for a pump means
only while it is running, so idle hours cost zero writes. Worst case ~168 writes/day, which NVS
wear-levelling absorbs comfortably; typical is far lower. Without the dirty flag this would be an
unconditional 144 writes/day forever for no benefit.

`begin()` loads the ring, checks `pwr.ver`, and restores `state.energyKwh`.

**Known limitation of the millis clock**, accepted per the time-base decision: on power loss the
ring resumes where it stopped, because nothing on the board can know how long it was dark. The
window becomes "the last 24 recorded hours" rather than a true wall-clock 24 hours. This does not
fabricate consumption — a dark controller means a dark pump — but the boundary drifts. NTP fixes it.

### `lib/SystemState/SystemState.h`

Add after the power block at `:125-131`:

```cpp
// ---- Rolling 24h stats (written by PowerStats, read by DisplayUI / CloudClient)
uint32_t stats24hRuntimeSec  = 0;
uint16_t stats24hCycles      = 0;
float    stats24hEnergyKwh   = 0.0f;
float    stats24hAvgCurrent  = 0.0f;   // mean amps while the pump was actually running
float    stats24hPeakCurrent = 0.0f;
```

**No `StateSnapshot` or `Field` entries, and no `SystemState.cpp` changes.** Unlike the
`tank-temperature.md` change, nothing here needs them: the six power fields are already fully wired,
and the display gate opens at least once a second anyway because `uptimeSeconds` is in the snapshot
and ticks every second (`main.cpp:78-81`) — which is exactly the stats' update rate. If CloudClient
later wants field-granular change detection on the 24h stats, that is the moment to extend
`StateSnapshot` and the three functions, following the pattern in `tank-temperature.md:35-42`.

### `lib/DisplayUI/DisplayUI.h`

Append to `FocusTarget` at `:15-18`, as the header comment at `:12-14` instructs:

```cpp
enum class FocusTarget : uint8_t {
    NONE,
    PUMP_TIMER,
    POWER_STATS
};
```

Declare `void _drawPowerStats(SystemState& state);` next to `_drawPumpTimer`, and the new shared
helper next to `_drawTimerText`:

```cpp
// Label hard left, value hard right, inside a box. The caller sets the font.
void _drawStatRow(int16_t boxX, int16_t boxW, int16_t y,
                  const char* label, const char* value, uint16_t valueColor);
```

### `lib/DisplayUI/DisplayUI.cpp`

The DOWN handler at `:52-57` is a hardcoded two-state toggle and UP at `:61-64` just drops to
`NONE`. Neither works with three targets. Replace with a real modular walk:

```cpp
static constexpr uint8_t FOCUS_COUNT = 3;   // keep in step with FocusTarget
case ButtonEvent::DOWN_PRESS:
    _focus = (FocusTarget)(((uint8_t)_focus + 1) % FOCUS_COUNT);
    break;
case ButtonEvent::UP_PRESS:
    _focus = (FocusTarget)(((uint8_t)_focus + FOCUS_COUNT - 1) % FOCUS_COUNT);
    break;
case ButtonEvent::LEFT_PRESS:
    _focus = FocusTarget::NONE;   // unchanged — LEFT stays the escape
    break;
```

**Assumption flagged**: this changes UP from "drop focus" to "walk backwards", which is the natural
behavior once there is more than one target to walk between. LEFT keeps its existing meaning as the
escape hatch, so nothing is lost.

**SELECT needs no change** — `:66-69` already guards on `_focus == FocusTarget::PUMP_TIMER`, so
`POWER_STATS` falls through as a no-op exactly as intended. `_applyIdleTimeout()` at `:78-89`
already clears `_focus` generically; also no change.

### `lib/DisplayUI/HomeUI.cpp`

New `_drawPowerStats()`, called from `_drawHome()` right after `_drawPumpTimer(state)` at `:282`.

Geometry `X=46, Y=52, W=112, H=62` → occupies x46..157, y52..113. This is the free region below the
pump circle (x45-75, y18-48) and the timer box (x80-159, y15-49). It clears the uptime line at
`(100,118)` (`:337`) and the tank column, which owns x0..44 down to y126.

```
┌────────────────────────────┐  y=52   drawRect, TFT_YELLOW when focused
│ Last 24h                   │  y=53   fillRect 110x11 in 0x2945 — the same grey
├────────────────────────────┤         as the title bar at HomeUI.cpp:61-62
│ Run                  02:14 │  y=68
│ Cyc                      7 │  y=80
│ kWh                  1.842 │  y=92
│ 4.3A  940W                 │  y=104  contextual
└────────────────────────────┘  y=113
```

Bottom row:
- pump **ON** → `"%.1fA  %.0fW"` from `state.current` / `state.powerWatts`, in `TFT_GREEN`
- pump **OFF** → `"avg %.1f  pk %.1f"` from the 24h stats, in `TFT_WHITE`

Font 1 (6x8) throughout, 12 px row pitch. Inner width 108 px = 18 characters; the longest string,
`avg 4.2  pk 5.1`, is 15. Fits.

Border mirrors the timer box exactly (`:193`): `TFT_YELLOW` when focused, `TFT_DARKGREY` otherwise.
No edit mode here, so no blink — the two boxes are told apart by which one is yellow.

When `state.powerFault` is set, draw the kWh row's value in `TFT_RED`. The box is now the only place
power health is visible, and the old full-width fault banner (`:326-331`) would collide with it.

**New `_drawStatRow()` helper.** There is currently no `drawBox`, no label/value row and no
right-align helper; the two existing helpers (`_drawTimerNum`, `_drawTimerText`, `:154-179`) are
timer-specific, and centering is done two inconsistent ways — `textWidth` math at `:123-125` versus
datum at `BootUI.cpp:38-41`. `_drawStatRow` right-aligns via `_sprite.textWidth(value)` and all four
rows use it.

Delete the dead commented-out power block at `:304-331` — it is superseded by this box and its
coordinates refer to a layout that no longer exists.

### `src/main.cpp`

- `#include "PowerMeter.h"` and `#include "PowerStats.h"` in the block at `:5-11`
- `PowerMeter powerMeter;` and `PowerStats powerStats;` at `:17-22`
- `setup()`: `powerMeter.begin()` and `powerStats.begin()` with their own boot steps
- `loop()` — insert into the pipeline at `:60-64`:

```cpp
inputManager.update(state);
radioReceiver.update(state);
powerMeter.update(state);    // NEW — self-gated sampling, must run every pass
pumpTimer.update(state);
sceneEngine.update(state);
pumpDriver.update(state);
powerStats.update(state);    // NEW — last, so it sees the settled pumpState
```

Ordering matters both ways: `powerMeter` must run every pass or the sample rate collapses, and
`powerStats` must run *after* `pumpDriver` so it observes the pump state this pass actually settled
on rather than last pass's.

### `docs/wiringe_guide.md`

The pin table at `:370-371` and sections 4 and 5 are already correct — no corrections needed. Add the
new constants to the embedded `config.h` snippets at `:141-146` and `:164-167` so the doc and the
header stay in sync, and add a **"Calibrating the power sensors"** section:

1. Enable `POWER_CAL_LOG`, flash, open the monitor.
2. Measure real mains with a multimeter. Set `ZMPT_CAL_V_PER_V = actual_mains_volts / Vrms_pin_volts`
   from the logged pin voltage. Reflash.
3. Run a known resistive load and compare watts against a plug-in energy meter. Adjust
   `ACS712_MV_PER_AMP` if the part deviates from its nominal 66 mV/A.

### `.claude/plans/milestones.md`, `.claude/plans/pump-timer.md`

Mark the Phase 4 power-sensing items done in `milestones.md` and note that the `powerFault` →
PumpDriver interlock is still outstanding. Update the key map at `pump-timer.md:41-56` with the new
three-target walk order.

---

## Out of scope — still open

- **PumpDriver refusing to start on `powerFault`.** `milestones.md` Phase 4 calls for it, but it is a
  pump *behavior* change and must not ship alongside brand-new, uncalibrated sensor math — a
  miscalibrated voltage reading would refuse to run the pump. `powerFault` is computed and displayed
  here; the interlock lands as its own change once the readings are trusted on hardware.
- `PUMP_MAX_RUN_MS` (the other half of `PumpDriver.h:17-18`'s `_runStartTimeMs`, still written and
  never read), scene hysteresis, Wi-Fi, cloud push. Phase 4/5 items unrelated to measurement.
- The 30-day view and the server push it depends on. The box is focusable and does nothing on
  SELECT, which is where that lands later — via the dormant `_goTo()` / `ScreenId` machinery at
  `DisplayUI.cpp:262-265`, which has never been called.
- Power factor is computed internally to get real power but is not stored in `SystemState` or shown.
  Add a field when there is somewhere to display it.

## Verification

1. **`pio run`** — compiles clean.

2. **Loop-rate sanity, before anything else.** Everything downstream depends on it. Temporarily count
   `PowerMeter` samples per window and log it; expect ~2000/sec. If materially lower,
   `analogReadMilliVolts` is too slow on this build — take the `analogRead` fallback and lower
   `ADC_SAMPLE_INTERVAL_US` to match. Do not trust a single reading until this checks out.

3. **Bench, sensors unpowered.** Expect `voltage ≈ 0`, `current` clamped to 0 by the noise floor, and
   `powerFault` latching true after 3 windows. Confirms the fault path and the noise floor both work
   before mains is anywhere near the board.

4. **Sensors powered, mains present, no load.** Run the calibration procedure above. After setting
   `ZMPT_CAL_V_PER_V` and reflashing, `voltage` should read within a volt or two of the multimeter
   and `frequency` ~50 Hz. Current should still be ~0.

5. **Known resistive load** (kettle, heat gun — PF ≈ 1, so watts should equal V×A): compare
   `powerWatts` against a plug-in energy meter. A resistive load cannot validate the power-factor
   path, so then check the pump itself — `powerWatts` should land meaningfully *below*
   `voltage × current`. If the two are equal on the pump, the `_sumVI` path is wrong.

6. **Stats, fast path.** Set `POWER_STATS_BUCKET_MS` to `60000` so an "hour" is a minute. Arm the
   pump timer with a short duty cycle (total `00:02`, break `00:01`, run `00:01`) via the editor at
   `HomeUI.cpp:181` to force several ON/OFF transitions quickly. Verify `Cyc` increments once per
   OFF→ON edge, `Run` accumulates only during ON slices and not during breaks, and `kWh` climbs only
   while running. Then leave it 24 minutes and confirm the ring rolls and the oldest bucket drops out
   of the totals.

7. **NVS.** With stats accumulated, hard-reboot. The box must repopulate from flash rather than
   zeroing, and `energyKwh` must survive. Then bump `POWER_STATS_NVS_VER` and reboot again — stats
   must reset cleanly instead of reading garbage.

8. **UI.** Press DOWN repeatedly: the walk is NONE → timer → power → NONE with the yellow border
   tracking it. UP walks the reverse; LEFT drops to NONE from either. SELECT on the power box does
   nothing while SELECT on the timer box still opens the editor. Leave it focused and confirm
   `UI_FOCUS_TIMEOUT_MS` (10 s) still drops focus. Watch the bottom row flip to green live readings
   the moment the pump starts and back to avg/pk when it stops.
