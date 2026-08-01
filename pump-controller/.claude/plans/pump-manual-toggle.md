# Manual pump ON/OFF — long-press SELECT

> Status: **implemented**. The relay polarity question in the verification
> section below is **resolved** — the module turned out to be 5V-logic and
> needs a BC547 driver, which inverts the pin. See
> [relay-level-shift.md](relay-level-shift.md). `RELAY_ACTIVE_LOW` is now `0`,
> so step 8 below no longer applies.

## Context

There is currently no way to turn the pump on. `InputManager::update()` is an
empty stub with a `// Phase 2` comment (`InputManager.cpp:13-16`) — no debounce,
no long-press, and it never writes to `SystemState`. `PumpDriver` has a working
`begin()` that parks the relay OFF, but its `update()` is empty and the class is
**not instantiated in `main.cpp` at all**. `SceneEngine::update()` is a no-op.

This wires the first real control path end to end: hold the middle (SELECT)
button for 1 second → pump toggles → relay switches → home screen reflects it.

The display needs no work. `pumpState` is already in the change-detection chain
(`SystemState.cpp:15`, `:35`, `:56`) and `_drawPumpState()` already renders the
ON/OFF circle (`HomeUI.cpp:110-126`), so the screen updates on its own at the
next 500 ms tick.

## Decisions

- **Relay polarity**: defaults to active-LOW behind a `RELAY_ACTIVE_LOW` define,
  and `begin()` logs which polarity is compiled in so the bench relay-click test
  reveals an inversion immediately.
- **Mode**: a long-press forces `OperatingMode::MANUAL` and toggles the pump, so
  the override survives Phase 4's AUTO hysteresis.
- **Safety**: `PUMP_MIN_OFF_MS` is enforced from day one.

## Data flow

Each module writes only its own fields, per the contract in `SystemState.h:76-80`:

```
InputManager  long-press SELECT  →  state.pumpRequest = TURN_ON/TURN_OFF
                                    state.mode        = MANUAL
SceneEngine   consumes request   →  state.desiredPumpAction, clears pumpRequest
PumpDriver    consumes action    →  drives relay, sets state.pumpState
DisplayUI     reads pumpState    →  redraw (already implemented)
```

## Changes

### 1. `include/config.h`

```c
// ---------- Input timing ----------
#define BUTTON_DEBOUNCE_MS       30
#define BUTTON_LONG_PRESS_MS   1000   // hold SELECT this long to toggle the pump

// ---------- Pump safety ----------
#define RELAY_ACTIVE_LOW          1   // 1 = LOW energises the relay
#define PUMP_MIN_OFF_MS       30000   // minimum OFF time before a restart is allowed
```

### 2. `lib/InputManager/InputManager.h` — event + per-button state

- Add `SELECT_LONG_PRESS` to `enum class ButtonEvent`.
- Add a private per-button state array (5 entries, indexed by a `Button` enum):
  `lastRaw`, `stable`, `lastChangeMs`, `pressStartMs`, `longFired`.

### 3. `lib/InputManager/InputManager.cpp` — real debounce + long-press

Buttons are `INPUT_PULLUP`, so **pressed reads LOW**. Table-driven over a
`static const uint8_t PINS[]` so all five share one code path, even though only
SELECT emits a long-press today.

Per button, per `update()`:

1. Read pin. If it differs from `lastRaw`, record `lastChangeMs = now` and
   update `lastRaw` — this is the bounce window, not a state change.
2. Once `now - lastChangeMs >= BUTTON_DEBOUNCE_MS` and the raw level still
   differs from `stable`, accept it:
   - **falling edge (press)**: `pressStartMs = now`, `longFired = false`
   - **rising edge (release)**: emit the short-press event **only if
     `!longFired`**, so a completed hold does not also fire a short press.
3. While held and not yet fired, `now - pressStartMs >= BUTTON_LONG_PRESS_MS`
   emits the long-press once and sets `longFired`.

On `SELECT_LONG_PRESS`, write the request directly into state:

```cpp
state.pumpRequest = (state.pumpState == PumpState::ON)
                  ? ActionRequest::TURN_OFF
                  : ActionRequest::TURN_ON;
state.mode = OperatingMode::MANUAL;
```

Note that short presses now fire on **release**, not press — required so a hold
can suppress them.

### 4. `lib/SceneEngine/SceneEngine.cpp` — manual passthrough

```cpp
void SceneEngine::update(SystemState& state) {
    if (state.pumpRequest != ActionRequest::NONE) {
        state.desiredPumpAction = state.pumpRequest;   // manual override wins
        state.pumpRequest       = ActionRequest::NONE; // consume
    }
    // Phase 4: AUTO hysteresis on tankLevelPct, paused while mode == MANUAL
}
```

### 5. `lib/PumpDriver/PumpDriver.cpp` — relay control + min-off interlock

- Private `void _writeRelay(bool on)` that resolves `RELAY_ACTIVE_LOW`, so
  polarity appears exactly once.
- `begin()`: park OFF via `_writeRelay(false)`, log the compiled polarity, and
  seed `_lastOffTimeMs = millis() - PUMP_MIN_OFF_MS`. **Without that seed the
  interlock would block the first start for 30 s after every boot**, since
  `_lastOffTimeMs` initialises to 0.
- `update()`: consume `desiredPumpAction`, then
  - OFF→ON: refuse (and log) while `millis() - _lastOffTimeMs < PUMP_MIN_OFF_MS`;
    otherwise energise, stamp `_runStartTimeMs`, set `state.pumpState = ON`.
  - ON→OFF: de-energise, stamp `_lastOffTimeMs`, set `state.pumpState = OFF`.
  - Already in the requested state: do nothing.

`PUMP_MAX_RUN_MS`, stale-tank refusal and power-fault refusal stay Phase 4;
`_runStartTimeMs` is stamped now so that work is a pure addition.

### 6. `src/main.cpp` — instantiate and schedule

- Add `PumpDriver pumpDriver;` and `SceneEngine sceneEngine;` instances.
- **`pumpDriver.begin()` runs first in `setup()`, before `displayUI.begin()`** —
  GPIO18 floats until it is configured, and parking the relay beats splash
  bookkeeping. It logs to serial only; `BOOT_TOTAL_STEPS` stays 3.
- In `loop()`, call `sceneEngine.update(state)` then `pumpDriver.update(state)`
  every iteration, before the display block. Both are guard-clause cheap.

## Known issue this does not fix

`main.cpp` calls `inputManager.update()` every iteration but reads
`inputManager.lastEvent()` only inside the 500 ms display block, so
`ButtonEvent`s are overwritten and lost before `DisplayUI` ever sees them. It
does not affect this feature — the pump path writes `state.pumpRequest`
directly rather than travelling as an event — but menu navigation will need
consume-on-read semantics. Flagging, not fixing.

## Verification

**Bench only — nothing connected to mains.** Relay click is the signal.

1. `pio run` — must compile clean.
2. `pio run -t upload && pio device monitor`.
3. Confirm the boot log names the compiled polarity and that the relay is quiet
   at boot (no click = parked correctly).
4. Hold SELECT (GPIO13) for 1 s → audible click, serial logs the transition,
   home screen circle flips to green ON within 500 ms.
5. Tap SELECT briefly → nothing happens; the short press must not toggle.
6. Hold again → click OFF, circle red.
7. Immediately hold a third time → serial logs the refusal and the relay stays
   silent, proving `PUMP_MIN_OFF_MS`. Wait 30 s, repeat, and it starts.
8. If step 3 clicks at boot, the module is active-HIGH: set
   `RELAY_ACTIVE_LOW` to 0 and reflash.
