# Pump timer — run window with an optional break cycle

> Status: **implemented**. Builds clean; hardware verification of the
> pump-on-budget behaviour (step 5 below) is the outstanding check.

## Context

`_drawPumpTimer()` painted two hardcoded strings — `"HH:MM"` and
`"HH:MM-HH:MM"` — with nothing behind them. This makes the box real: the user
sets how long the pump should run, optionally sets a break cycle inside that,
and confirms with SELECT.

The feature also fixed the button-event loss flagged as a known issue in
[pump-manual-toggle.md](pump-manual-toggle.md) — the timer UI is driven by
events, so it was unusable until that was closed.

## What the two rows mean

**Row 1 — the run budget.** How long the pump will be **ON**, in total. This is
accumulated pump-on time, *not* wall clock: a break pauses the countdown rather
than eating into it.

**Row 2 — `BREAK - RUN`.** How long the pump stops, and how long it runs
between stops. Blank means no cycle: the pump runs straight through.

Worked example — row 1 `01:00`, row 2 `00:10-00:30`:

| Wall clock | Phase | Budget left |
|---|---|---|
| 0–30 min | RUN | 60 → 30 min |
| 30–40 min | BREAK | 30 min (parked) |
| 40–70 min | RUN | 30 → 0 min |
| 70 min | finished, pump OFF | — |

One hour of pumping, spread over 70 minutes. The cycle does **not** have to
divide the budget — a final run slice is simply cut short when the budget runs
out, and the timer always ends on a run, never on a trailing break.

## Key map

Getting in takes two steps: DOWN **focuses** the timer box — the border turns
yellow, nothing else changes — and SELECT then opens the editor, putting a
blinking cursor on row 1's `HH`. The border says the box has the buttons; the
cursor is what says it is being edited. The split exists so DOWN stays free to
mean "next widget" as more of the home screen becomes focusable (`FocusTarget`
in `DisplayUI.h` is that walk order).

The walk is now a real cycle: `NONE → PUMP_TIMER → POWER_STATS → NONE`. `NONE`
is a position in it rather than an escape from it, so a full circuit returns
where it started. A new widget is added by appending to `FocusTarget` before
`_COUNT` — `_handleNavigation` does not change.

| Key | While focused, not editing |
|---|---|
| DOWN | next focusable widget, wrapping through NONE |
| UP | previous focusable widget, wrapping. Once there is more than one widget this has to step back rather than drop out, or the last one is unreachable without a full circuit |
| LEFT | drops focus — the escape hatch |
| SELECT | enters edit on the timer. Nothing on the stats box: it is focusable so the walk order is settled, but has no detail view until the server-backed history exists |
| SELECT held 1 s | unchanged pump toggle / timer cancel — focus is only a highlight, so `uiEditing` stays false and InputManager keeps its override |

| Key | While editing |
|---|---|
| UP / DOWN | +1 / −1 on the focused field, wrapping. Never moves between fields |
| RIGHT | next field; from row 1's `MM` it opens row 2 |
| LEFT | previous field; on row 1's `HH` it exits, discarding |
| SELECT | validate, commit, start — the only thing that starts anything |
| SELECT held 1 s | exits, discarding — same as LEFT off the first field |

Every way out of edit — LEFT off the first field, the long press, or a
successful commit — drops the focus as well, so the box is never left highlighted
with nothing to do.

Both states also back out on their own once the buttons go quiet:
`UI_FOCUS_TIMEOUT_MS` for a focused box, `UI_EDIT_TIMEOUT_MS` for an open edit
(longer, since setting a timer has natural pauses). The check runs at the display
cadence, so it is accurate to `INTERVAL_DISPLAY_MS`.

Backing out never disturbs a timer that is already running — by hand or by
timeout, only the edit is dropped. Reaching row 2 does not start anything on its
own.

Minutes step by 1 (`TIMER_MINUTE_STEP`), hours wrap at `TIMER_MAX_HOURS`.
Stepping **down** out of a never-touched field lands on `00` rather than
wrapping to the top, since down-from-nothing reading as 23 is jarring.

## Display states

| State | Row 1 | Row 2 |
|---|---|---|
| Nothing armed | `HH:MM` dim | `HH:MM-HH:MM` dim |
| Focused, not editing | whatever the box would show anyway — only the border changes, to yellow | as above |
| Editing | working values, focused field yellow + blinking | as above, live once row 2 is opened |
| Running | `HH:MM:SS` white, counting the budget down | the committed cycle, or dim placeholder if none |
| On a break | `HH:MM:SS` **orange**, counting the break down | as above |

Seconds appear only while running. The row is measured and centred in the box
because `HH:MM:SS` does not fit at a fixed start x.

The orange break countdown exists because the budget is parked during a break —
a frozen number would read as a hang. One number always moves; its colour says
which clock it is.

**Placeholder labels are per row, not per field.** A field shows `HH`/`MM` only
while its whole row is untouched; once any field in the row is set, its siblings
render as the `00` they will commit as. A `_timerTouched` bitmask tracks this, so
a deliberate `00` is never mistaken for "unset".

## Validation

`DisplayUI::_dutyValid()` runs on SELECT. Deliberately permissive — the user
should not have to do arithmetic:

| Rejected | Why |
|---|---|
| row 1 total `00:00` | an empty window is not a timer |
| exactly one row-2 field set | half a duty cycle is not one |
| run ≥ total | the budget is spent before the first break, so row 2 could never fire |

Everything else commits. There is deliberately **no** break-vs-total rule: a
break costs the budget nothing, so even a break longer than the whole window is
coherent — the pump just waits it out and then spends the rest of its budget.

On rejection the row-2 fields are cleared back to blank labels and the cursor
lands on the break `HH`, staying in edit. Serial logs
`[DisplayUI] duty cycle rejected - ...`.

## Data flow

Follows the existing contract — each module writes only its own fields:

```
DisplayUI    SELECT commits    →  state.timerTotalSec/BreakSec/RunSec
                                  state.timerRequest = START
InputManager long-press SELECT →  state.timerRequest = CANCEL   (only when not editing)
PumpTimer    consumes request  →  counts down, drives state.pumpRequest
SceneEngine  consumes request  →  state.desiredPumpAction
PumpDriver   consumes action   →  relay + state.pumpState
DisplayUI    reads timerPhase / timerRemainSec / timerPhaseSec → redraw
```

`PumpTimer::update()` runs **before** `sceneEngine.update()` in `loop()` so the
request it emits is consumed the same iteration.

## Modules

### `lib/PumpTimer/` (new)

Same shape as `SceneEngine` — plain `update(SystemState&)`, no constructor args.

- **Tick** advances `_lastTickMs` by a fixed 1000 ms rather than resetting to
  `now()`, so a long loop iteration cannot make the countdown drift.
- **`timerRemainSec` only decrements while `RUNNING`**, and expiry is likewise
  only checked while running — that is what makes row 1 a pump-on budget.
- **Only run slices are capped** by the remaining budget; a break always runs its
  full length.
- **The pump is re-asserted every tick**, not just at phase edges:

```cpp
PumpState want = (state.timerPhase == TimerPhase::RUNNING) ? PumpState::ON : PumpState::OFF;
if (state.pumpState != want && state.pumpRequest == ActionRequest::NONE) {
    state.pumpRequest = (want == PumpState::ON) ? ActionRequest::TURN_ON
                                                : ActionRequest::TURN_OFF;
}
```

  This matters because `PumpDriver::update()` *silently* refuses a start while
  `PUMP_MIN_OFF_MS` is counting. A one-shot request at the phase edge would be
  swallowed and the run slice would never start; re-asserting means the pump
  comes on as soon as the interlock clears.

### `lib/InputManager/` — event latching

`update()` used to clear `_lastEvent` at the top of every call while `main.cpp`
called it every iteration and only read it every 500 ms, so nearly every press
was lost. Events are now latched until `takeEvent()` collects them, and
`main.cpp` redraws immediately on an event instead of waiting for the tick:

```cpp
ButtonEvent event = inputManager.takeEvent();
if (event != ButtonEvent::NONE) {
    displayUI.update(state, event);
    lastDisplayMs = now;
} else if (now - lastDisplayMs >= INTERVAL_DISPLAY_MS) {
    displayUI.update(state, ButtonEvent::NONE);
    lastDisplayMs = now;
}
```

The long-press branch now checks `state.uiEditing` first: while the edit UI owns
the buttons it uses the long press to back out, so the press must not reach
through to the pump or a running timer. Outside edit mode the old behaviour
stands — cancel a running timer, otherwise toggle the pump.

### `lib/SystemState/` — new fields

```cpp
enum class TimerRequest : uint8_t { NONE, START, CANCEL };
enum class TimerPhase   : uint8_t { IDLE, RUNNING, BREAKING };

TimerRequest timerRequest   = TimerRequest::NONE;
uint32_t     timerTotalSec  = 0;    // the run budget, 0 = nothing armed
uint32_t     timerBreakSec  = 0;    // how long the pump breaks for
uint32_t     timerRunSec    = 0;    // how long it runs between breaks
bool         uiEditing      = false;
TimerPhase   timerPhase     = TimerPhase::IDLE;
uint32_t     timerRemainSec = 0;    // budget left
uint32_t     timerPhaseSec  = 0;    // seconds left in the current slice
```

`timerPhase` and `timerRemainSec` joined `StateSnapshot`, the `hasChanged()`
OR-chain, `markSeen()`, and `Field::PUMP_TIMER`.

`timerPhaseSec` is deliberately *not* in the snapshot — `uptimeSeconds` already
forces a redraw every second, which is enough to tick the break countdown.

### `lib/DisplayUI/` — edit state

`_timerField` (a `TimerField` cursor) doubles as the "am I editing" flag.
`_handleTimerEdit()` runs before the change-detection early-return in `update()`,
which also forces a redraw while editing so the 500 ms blink is not suppressed
by the 1 s `uptimeSeconds` cadence.

## config.h

```c
// ---------- Pump timer ----------
#define TIMER_MINUTE_STEP         1   // minutes per UP/DOWN press (0..59)
#define TIMER_MAX_HOURS          23   // hour field wraps 0..23
```

## Known limitation

`PUMP_MIN_OFF_MS` is 30 s, so a break shorter than that is partly eaten by the
restart interlock — a 1-minute break spends its first half waiting. The pump
still returns (PumpTimer re-asserts every tick), just late into the following
run slice. Minute-granularity entry means this only bites at the 1-minute end of
the range.

## Verification

1. `pio run` — compiles clean.
2. `pio run -t upload && pio device monitor`.
3. Press each button once at the home screen and confirm the UI reacts to
   **every** press — that alone proves the latched-event fix.
4. DOWN → row 1 blinks on `HH`. UP/DOWN change it, LEFT/RIGHT move to `MM` and
   back, LEFT on `HH` exits with no change.
5. **The pump-on budget.** Set total `00:04`, row 2 `00:01-00:02`, SELECT:
   - `[PumpTimer] armed - 240s window, duty cycle`, pump ON, row 1 white.
   - At 2 min: `[PumpTimer] break for 60s (120s left in window)`, pump OFF, row 1
     turns orange and counts `00:01:00` down.
   - At 3 min: pump ON, row 1 white and **resuming at `00:02:00`** — if it resumes
     at `00:01:00` the budget leaked during the break.
   - Finishes ~5 min wall clock with 4 min of pumping. Check with a stopwatch.
6. No duty cycle (SELECT straight from row 1): counts down continuously, never
   turns orange.
7. Commit total `00:00` → refused, stays in edit. Commit `00:04` with row 2
   `00:01-00:10` → rejected (run ≥ total), row 2 cleared, cursor on break `HH`.
8. Long-press SELECT mid-run → timer cancelled, pump OFF, row 1 back to the dim
   placeholder, mode back to AUTO. Long-press *while editing* → drops the edit
   only, a running timer keeps running.
