# UI and button guide

The permanent reference for what every button does on every screen. Wiring lives
in [wiringe_guide.md](wiringe_guide.md); this is behaviour only.

**Keep this in step with the code.** Every key map below is transcribed from
`lib/DisplayUI/DisplayUI.cpp` and `lib/InputManager/InputManager.cpp`. If you
change a handler, change this file in the same commit.

---

## 1. The buttons

Five buttons, active-low with internal pull-ups. Pins from `include/config.h`:

| Button | GPIO | Role |
| ------ | ---- | ---- |
| LEFT   | 11   | back one level / escape |
| UP     | 12   | previous item, increment |
| SELECT | 13   | confirm, commit, enter |
| RIGHT  | 14   | forward one level, next field |
| DOWN   | 6    | next item, decrement |

There is **no BACK button**. LEFT serves as back, and a LEFT hold is the jump
home.

### Press types

A press is debounced for 30 ms (`BUTTON_DEBOUNCE_MS`). Short presses fire **on
release**; long presses fire **mid-hold**, once, as soon as the threshold passes.
A completed hold never also fires a short press.

| Gesture | Hold time | Constant |
| ------- | --------- | -------- |
| Short press | — | fires on release |
| SELECT held | 1 s | `BUTTON_LONG_PRESS_MS` |
| LEFT held, off the home screen | 2 s | `BUTTON_BACK_HOLD_MS` |
| LEFT held, on the home screen | 3 s | `BUTTON_BYPASS_HOLD_MS` |

UP, RIGHT and DOWN have no long press at all, and no auto-repeat — each step is
one press.

**Why LEFT has two hold times.** Its hold means "back to home" everywhere except
home, where there is nowhere further back to go and it toggles bypass instead.
Returning home is harmless navigation and should be quick; arming bypass changes
how the pump behaves and should take a hold nobody does by accident.

---

## 2. Screen map

Screens are a left/right strip. LEFT is always back one level, RIGHT always
forward one.

```
   HOME  ──RIGHT──▶  CONFIG  ──SELECT──▶  CONFIG_ITEM
     ◀────LEFT────      ◀─────LEFT────
     ▲                       ▲
     └───── LEFT held 2s ────┴──────────────────┘
```

The boot screen is not part of this — it is a setup-time sequence, not a screen
the navigation can reach.

---

## 3. Home screen

```
┌──────────────────────────────────────────────┐
│                        ⊘   ☁   ●   ▁▃▅▇     │ y0-13   title bar
├────────┬─────────────────────────────────────┤
│        │      ┌──────────────────────┐       │
│  ████  │  ON  │ HH:MM                │       │ y15-49  pump + timer
│  ████  │      │ HH:MM-HH:MM          │       │
│  ████  │      └──────────────────────┘       │
│  ████  ├─────────────────────────────────────┤
│  ████  │ Last 24h                            │
│   47%  │ Run              02:14:07           │ y52-113 24h stats
│        │ Cyc                     7           │
│  24.3C │ kWh                 1.842           │
│  ████  │ avg 4.2A pk 5.1A                    │
│  ████  ├─────────────────────────────────────┤
│  ████  │                        00:41:12     │ y118    uptime
└────────┴─────────────────────────────────────┘
 x0-44    x45-75  x80-159
 tank     pump    timer / stats
```

### Title bar icons

Drawn on **every** screen, not just home — they are global state.

| Icon | x | Meaning |
| ---- | - | ------- |
| ⊘ ban | 100 | **Bypass.** Dim grey = off. Red = on, AUTO level control standing down. |
| ☁ cloud | 118 | MQTT broker. Dim grey = disconnected, green = connected. |
| ● dot | 136 | Radio heartbeat, blinks once a second. Green = tank packets arriving, red = stale. |
| ▁▃▅▇ bars | 147 | Wi-Fi signal, 0–4 bars from RSSI. |

### Key map — home, nothing focused

| Key | Action |
| --- | ------ |
| DOWN | focus the next widget |
| UP | focus the previous widget |
| LEFT | drop focus (no-op if nothing is focused) |
| RIGHT | open the config page |
| SELECT | nothing |
| SELECT held 1 s | toggle the pump — or cancel a running timer, if one is armed |
| LEFT held 3 s | toggle bypass |

The focus walk is a cycle that includes "nothing focused", so a full circuit
returns where it started:

```
none ──DOWN──▶ pump timer ──DOWN──▶ 24h stats ──DOWN──▶ none
```

Focus is only a highlight — the box border turns yellow and nothing else
changes. SELECT is what commits to editing. This split keeps DOWN free to mean
"next widget" as more of the screen becomes focusable.

The 24h stats box is focusable but SELECT does nothing on it. It is in the walk
so the order is settled before it gets a detail view.

**The stats box's bottom row swaps with the pump.** While the pump runs it shows
live `230V 4.2A 940W` in green — supply health only means anything under load,
since a line that reads fine idle can sag hard the moment the motor pulls. With
the pump off it shows the 24h `avg 4.2A pk 5.1A` instead. The kWh row turns red
on a power fault; that box is the only place power health is visible.

### SELECT held 1 s — what it does depends on what is running

| State | Effect |
| ----- | ------ |
| A timer is running | cancel the timer; the pump stops with it |
| No timer, pump off | pump ON, mode switches to MANUAL |
| No timer, pump on | pump OFF, mode switches to MANUAL |
| Any editor is open | nothing — the UI owns the buttons |

---

## 4. Pump timer editor

Reached from home: DOWN to focus the timer box, then SELECT. The border stays
yellow and a blinking cursor appears on row 1's `HH`.

```
┌──────────────────────┐
│ HH:MM                │  row 1 — the total run window
│ HH:MM-HH:MM          │  row 2 — break / run duty cycle (optional)
└──────────────────────┘
    ▲     ▲
  break  run
```

Row 1 is how long the pump should run in total. Row 2 is optional: a break slice
and the run slice it follows, repeating until the window runs out. Leave row 2
blank and the pump runs straight through.

| Key | Action |
| --- | ------ |
| UP / DOWN | +1 / −1 on the cursor field, wrapping. Never moves between fields. |
| RIGHT | next field. From row 1's `MM` it opens row 2. |
| LEFT | previous field. On row 1's `HH` it exits, discarding. |
| SELECT | validate, commit, and start the timer |
| SELECT held 1 s | exit, discarding — same as LEFT off the first field |
| LEFT held | **nothing.** The editor owns LEFT; the bypass shortcut stands down. |

Hours wrap 0–23, minutes step by 1 and wrap 0–59.

**Placeholder labels are per row.** A field shows `HH`/`MM` only while its whole
row is untouched; once any field in the row is set, its siblings read as the `00`
they will commit as. A deliberate `00` is never mistaken for "unset".

**Rejected commits.** A window of `00:00` is not a timer — SELECT does nothing
and you stay in the editor. A half-filled duty cycle (a break with no run, or a
run at least as long as the whole window) hands row 2 back blank with the cursor
on it rather than starting something nonsensical.

**Exiting never stops a running timer.** LEFT, the SELECT hold and the idle
timeout all discard the *edit* only. Cancelling an armed timer is the SELECT hold
from an unfocused home screen.

---

## 5. Config page

RIGHT from home. A plain list: setting name left, current value right, selected
row on a grey bar.

```
┌──────────────────────────────────────────────┐
│ CONFIG                 ⊘   ☁   ●   ▁▃▅▇     │
├──────────────────────────────────────────────┤
│▐Tank Full                        0.300 m    ▌│  ◀ selected
│ Tank Empty                       1.500 m     │
│ Bypass                               OFF     │
└──────────────────────────────────────────────┘
```

| Key | Action |
| --- | ------ |
| UP / DOWN | move the selection, wrapping at both ends |
| SELECT | open the editor for that row |
| LEFT | back to home |
| RIGHT | nothing — there is no page past this one yet |
| LEFT held 2 s | back to home |
| SELECT held 1 s | nothing — the pump cannot be reached from here |

Unlike the home screen's focus walk there is no "nothing selected" position: a
row is always selected.

### The settings

| Setting | Type | Meaning |
| ------- | ---- | ------- |
| Tank Full | metres | Distance the sensor reports with the tank **full** |
| Tank Empty | metres | Distance the sensor reports with the tank **empty** |
| Bypass | on/off | ON = AUTO level control stands down |

**Calibrating the tank.** These are measured, not calculated — no tape measure
and no tank geometry. Watch the serial log with the tank full, then empty:

```
[RadioReceiver] seq=42 dist=812mm -> 47% temp=24.3C flags=0x00
```

Enter each `dist` reading. The log is in millimetres and the page is in metres,
so `812mm` is `0.812`. Empty must be the larger number — the sensor looks down at
the water, so less water means a longer echo.

**What bypass does.** ON means `SceneEngine` takes no level-driven action: the
pump answers only to the buttons, the timer and the cloud. It does **not** reach
`PumpDriver` — the minimum-off-time and other safety interlocks apply either way.

---

## 6. Setting editor

SELECT on a config row. The value fills the screen with the key map spelled out
underneath.

```
┌──────────────────────────────────────────────┐
│ TANK FULL              ⊘   ☁   ●   ▁▃▅▇     │
├──────────────────────────────────────────────┤
│                                              │
│                 0.300 m                      │  y46, font 4
│                 ▔                            │  blinking digit
│                                              │
│  UP/DN digit   L/R move                      │  y104
│  SELECT save   LEFT back                     │  y114
└──────────────────────────────────────────────┘
```

### Numeric settings — `M.MMM m`

Four digits, edited one at a time. The cursor digit blinks yellow; the rest stay
white.

| Key | Action |
| --- | ------ |
| UP / DOWN | cycle the cursor digit 0–9, wrapping |
| RIGHT | next digit |
| LEFT | previous digit. On the first digit it exits, discarding. |
| SELECT | validate, commit, back to the list |
| SELECT held 1 s | back to the list, discarding |
| LEFT held 2 s | back to **home**, discarding |

Digit-at-a-time rather than a spinner because a 1 mm step over a 4.5 m range is
unusable.

**Validation happens on commit, not while typing.** You can dial through
out-of-range values freely. SELECT rejects anything outside 0.200–4.500 m, or any
value that would put Full at or past Empty. A rejection puts the cursor back on
the first digit, logs why, and leaves the stored value untouched.

### On/off settings

| Key | Action |
| --- | ------ |
| UP / DOWN | toggle |
| SELECT | commit, back to the list |
| LEFT | exit, discarding |

There is no cursor to move, so LEFT exits immediately rather than stepping.

---

## 7. Timeouts

The display never sits waiting on someone who walked away.

| State | Timeout | Constant | Result |
| ----- | ------- | -------- | ------ |
| A home widget is focused | 10 s | `UI_FOCUS_TIMEOUT_MS` | focus drops |
| The timer editor is open | 30 s | `UI_EDIT_TIMEOUT_MS` | edit discarded, focus drops |
| Any config screen is open | 30 s | `UI_EDIT_TIMEOUT_MS` | back to home, edit discarded |

Editing gets the longer window — setting a timer has natural pauses. A running
timer is never affected by any of these.

---

## 8. Persistence

Committing a setting writes it to NVS on the same loop pass, whether it came from
the config page or the bypass shortcut. Nothing is lost on a power cycle, and
there is no "save" step beyond SELECT.

The pump timer is **not** persisted — an armed timer does not survive a reboot.

---

## 9. Quick reference

| | HOME | CONFIG | CONFIG_ITEM | Timer editor |
|-|------|--------|-------------|--------------|
| **UP** | prev widget | prev row | toggle / digit +1 | field +1 |
| **DOWN** | next widget | next row | toggle / digit −1 | field −1 |
| **LEFT** | drop focus | → home | prev digit / exit | prev field / exit |
| **RIGHT** | → config | — | next digit | next field |
| **SELECT** | edit timer | open row | commit | commit + start |
| **SELECT 1 s** | pump / cancel timer | — | exit | exit |
| **LEFT 2 s** | — | → home | → home | — |
| **LEFT 3 s** | toggle bypass | — | — | — |

Two things worth memorising:

- **Hold LEFT to get home** from anywhere you are lost.
- **Hold SELECT on the home screen** to force the pump on or off.
