# Relay level-shift — BC547 driver + polarity flip

> Status: **firmware and docs done** — builds clean. Blocked on the BC547 being
> wired before it can be tested.

## Context

The pump relay never releases. Bench measurement proved the firmware is
correct: GPIO18 reads 3.3V when the UI shows OFF and 0.02V when it shows ON.
The fault is electrical. The JQC-3F-05VDC-C module runs its input stage from
5V, wired `VCC → resistor → opto LED → IN`. With IN driven to 3.3V there is
still 1.7V across that branch — above the opto's ~1.2V forward drop — so the
LED keeps conducting and the relay stays energised. A 3.3V pin cannot turn it
off, and no firmware change can fix that.

The module has no VCC/JD-VCC jumper, so the split-rail fix is unavailable.

**Chosen fix: a BC547 NPN low-side switch, identical to the backlight driver
already proven on this board** (`docs/wiringe_guide.md:40-49`). Same part, same
resistor value, same topology.

This also corrects two comments committed in `2dca6aa` claiming the pump runs
from reset — false then, and about to be false in a different way.

## Hardware (wire this before flashing)

```
   ┌───────┐
   │ BC547 │   ← flat side toward you, legs down
   └┬──┬──┬┘
    C  B  E
```

**Remove the existing GPIO18 → IN wire first.**

| # | From | To |
|---|------|-----|
| 1 | BC547 **left leg** (C) | Relay module **IN** |
| 2 | BC547 **right leg** (E) | **GND** |
| 3 | BC547 **middle leg** (B) | **3.3kΩ** → **GPIO18** |
| 4 | BC547 **middle leg** (B) | **21.5kΩ** → **GND** |
| 5 | Relay **VCC** | **5V** |
| 6 | Relay **GND** | **GND** — common with the ESP32 |

Both resistors land on the middle leg: one up to GPIO18, one down to GND.

The transistor is a remote-controlled jumper wire between IN and GND. No pull-up
on IN is needed — leaving IN open *is* the OFF condition, confirmed by hand with
a jumper before building the circuit.

**Why the 3.3kΩ.** The base is a diode clamping at ~0.7V that then stops
resisting. Direct from a GPIO that is a short in all but name:
`(3.3 − 0.7) / ~40Ω ≈ 65mA` against a pin rated 20mA continuous, 40mA absolute.
With 3.3kΩ it is 0.79mA — roughly 40× the ~0.02mA needed to saturate for a 4mA
load at hFE 200-400. Not a 3.3V-specific requirement; 5V would need it more.

**Why the 21.5kΩ.** Carries no working current — it defines the idle state.
GPIO18 is Hi-Z from reset until `pinMode()` runs, and a floating base can pick up
enough stray to partially conduct. Harmless flicker on the backlight, a spurious
pump start here. Costs 0.033mA of base drive, about 4%.

Values chosen from the parts on hand. The **ratio** matters more than the
absolutes — keep the pull-down 5-10× the base resistor. 3.3kΩ : 21.5kΩ is 6.5×.
Alternatives: 4.6kΩ + 46kΩ, or 1kΩ + 10kΩ.

**Staged bench test, before connecting GPIO18:** wire 1, 2, 4, 5, 6 and leave the
3.3kΩ's far end loose. Power the module — loose = relay off; touch the loose end to
3.3V = click on; remove = click off. Then plug it into GPIO18. If it behaves
backwards, the legs are mirrored — rotate the transistor 180°.

## Polarity

The transistor inverts. At the module, IN LOW is still ON — that does not
change. At the pin:

```
GPIO18 HIGH → base current → transistor conducts → IN at 0V → relay ON
GPIO18 LOW  → no base current → transistor open   → IN at 5V → relay OFF
```

So `RELAY_ACTIVE_LOW` flips `1` → `0`. Nothing above `PumpDriver::_writeRelay()`
changes; that define exists to absorb exactly this.

## Changes

### 1. `include/config.h`

Flip `RELAY_ACTIVE_LOW` to `0`, and replace the comment with the reason — a
BC547 low-side switch inverts the pin, so HIGH now energises the relay. Without
that note the value looks like a mistake, since the module itself is active-LOW.

### 2. `lib/PumpDriver/PumpDriver.cpp` — `begin()`

Remove the "pump is RUNNING from the moment the ESP32 comes out of reset"
comment. With the BC547 plus base pull-down the relay is held off through reset
by the hardware. Keep `pinMode()` + `_writeRelay(false)` — driving the pin to a
known state remains correct, it is simply belt-and-braces rather than a race.

### 3. `src/main.cpp` — `setup()`

Same correction to the "FIRST … the pump runs from reset until this call"
comment. `pumpDriver.begin()` stays the first statement as defensive ordering.

### 4. `docs/wiringe_guide.md` — section 6

`:179` currently reads `IN | GPIO18 | Direct — module's transistor handles
switching`, which is now wrong. Replace with the BC547 driver circuit, drawn in
the same style as the backlight circuit at `:40-49`, and record why: 5V-logic
module, 3.3V GPIO cannot reach its HIGH threshold. Replace the "verify
active-HIGH or active-LOW" note at `:189-191` — that question is settled, and
the answer at the pin is now HIGH = ON.

### 5. `.claude/plans/pump-manual-toggle.md`

Add a short note that the relay polarity question raised in its verification
section is resolved, pointing at this plan.

## Verification

**Bench only — mains disconnected.**

1. `pio run` — must compile clean.
2. Wire the BC547, the 3.3kΩ base resistor and the 21.5kΩ pull-down. Confirm the
   emitter shares ground with the ESP32.
3. Power up and watch through boot: the relay must stay silent and the module's
   green trigger LED must be **off**. Previously it was permanently lit — that
   LED going dark is the proof the fix worked.
4. Long-press SELECT → click, green LED on, home screen circle green within
   500 ms.
5. Long-press again → click, green LED off, circle red.
6. Immediately long-press a third time → serial logs the `PUMP_MIN_OFF_MS`
   refusal and the relay stays silent.
7. If the relay now behaves backwards — energised at idle, releasing on
   long-press — the transistor is wired as an emitter follower rather than a
   low-side switch. Check collector goes to IN and emitter to GND, not swapped.
