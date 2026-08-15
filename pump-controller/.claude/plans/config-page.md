# Config page — a second screen for the settings that outlive a reboot

> Status: **implemented**. Builds clean; all hardware verification below is
> outstanding — no board was connected when the code landed.

## Context

The firmware had exactly one screen. `ScreenId` held only `HOME`, `_goTo()` was
dead code, and `_getScreenTitle()` returned `nullptr`. The multi-screen skeleton
existed but had never been exercised.

Meanwhile the tank calibration was a pair of compile-time placeholders marked
`TODO` — `TANK_DISTANCE_FULL_MM` / `TANK_DISTANCE_EMPTY_MM` — so calibrating a
real tank meant a reflash. [radio-receiver.md](radio-receiver.md) deferred that
to "the next plan", and `RadioReceiver` already carried a
`tankEmptyMm > tankFullMm` guard written for this moment.

This is that plan. It delivers the second screen, the navigation around it, a
table-driven settings list, a per-setting editor, and NVS persistence. Two
settings ship in it: the two tank calibration distances, and a bypass flag.

## Decisions

### Screens are a left/right strip

`ScreenId` is now `HOME → CONFIG → CONFIG_ITEM`. LEFT is always back one level,
RIGHT always forward one. A new page is added by appending to the enum and giving
it a case in the event routing and the draw switch.

| Screen | LEFT | RIGHT | UP / DOWN | SELECT | LEFT held 2 s |
|---|---|---|---|---|---|
| `HOME` | drop focus | → `CONFIG` | walk `FocusTarget` | enter timer edit | drop focus |
| `CONFIG` | → `HOME` | inert (no page past this yet) | move selection, wrapping | → `CONFIG_ITEM` | → `HOME` |
| `CONFIG_ITEM` | previous digit; off the first → `CONFIG`, discarding | next digit | change digit / toggle | validate, commit, → `CONFIG` | → `HOME`, discarding |

The 2 s LEFT hold is the only global gesture and is handled in `update()` before
the per-screen routing, so it works from any depth. It delivers the
"long-press BACK from any screen" line in [milestones.md](milestones.md) Phase 2.

**Why 2 s and not the existing 1 s.** LEFT already means "back one level" on a
short press, so the hold that skips all the way home has to be unmistakably
longer than a press that overshot. `BUTTON_BACK_HOLD_MS` is its own constant.

### Long press is now per-button

`InputManager` had one global `BUTTON_LONG_PRESS_MS` and gated the whole
long-press block behind `if (i == BTN_SELECT)`. It now carries `LONG_MS[]` and
`LONG_EVENTS[]` tables parallel to the existing `SHORT_EVENTS[]`, with `0`
meaning "this button has no long press". SELECT's pump-toggle / timer-cancel side
effects stayed exactly where they were, still behind `i == BTN_SELECT` — the
generalisation is only about which events exist.

A button with `LONG_MS == 0` is skipped entirely rather than firing and being
discarded, so its hold never suppresses the short press that fires on release.

### The UI owns the buttons off home

`state.uiEditing` used to mean "the timer editor is open". It now means
`_editing() || !_onHome()`. Without this, a 1 s SELECT hold inside a settings page
would reach through `InputManager` and toggle the pump.

### Settings are a table

`ConfigItem` + `CONFIG_DEFS[]` (`ConfigUI.cpp`), with a `static_assert` tying
them together. Adding a setting is one enum member and one table row — neither
the list walk nor the editor routing changes. Same "append and nothing else
moves" property `FocusTarget` has.

`ConfigKind` picks the editor: `METRES` for a millimetre value, `BOOL` for a
toggle.

### Metres are edited a digit at a time

Values are stored as `uint16_t` millimetres and shown as `M.MMM m` — three
decimals is the exact value at millimetre resolution, not a rounding. The editor
puts a blinking cursor on one of the four digits; UP/DOWN cycles it 0–9 and
LEFT/RIGHT moves. This is the digit-place variant of the timer's field cursor
rather than a single spinner, because a 1 mm step over a 4.5 m range is unusable.

Out-of-range values are reachable mid-edit on purpose. Validation happens on
commit, the same permissive-edit split `_dutyValid()` uses for the timer:

- `TANK_MIN_MM` (200) ≤ value ≤ `TANK_MAX_MM` (4500) — the AJSR04M blind zone and
  rated maximum, from `water_tank/include/config.h`.
- The pair must stay ordered: empty is the further reading, because the sensor
  looks down at the water and less water means a longer echo.

A rejected commit stays on the editor with the cursor back on the first digit and
logs why — the same "hand it back rather than start something nonsensical" the
timer does with an invalid duty cycle.

### Bypass is scoped to AUTO, not to safety

Bypass ON stands the AUTO level logic down: the pump then answers only to the
buttons, the timer and the cloud. The guard sits in `SceneEngine::update()`
**below** the manual-request consumption, so bypass can never disarm the
override, and it does not reach `PumpDriver` — rule 3 in
[milestones.md](milestones.md) is explicit that the safety interlocks there are
never bypassed. This is narrower than the v1 `control_box.cpp` flag, which also
skipped the full-tank stop.

The config page row is not the only way in: a title-bar icon and a LEFT-hold
shortcut on the home screen were added on top of this — see
[bypass-shortcut.md](bypass-shortcut.md).

### Persistence goes through the state, not a call

`DisplayUI` sets `state.configDirty` on commit; `ConfigStore::update()` sees the
flag, writes, and clears it. No module calls another — the contract in
`SystemState.h` holds.

`ConfigStore` mirrors `PowerStats::_load()/_save()`, which set the project's NVS
convention: namespace `pumpctl`, keys prefixed by the owning module, a version
key checked before anything is trusted. Unlike `PowerStats` there is no flush
timer — settings change by hand, so there is no wear to spread out and no reason
to make the user wait for the write.

A restored calibration is re-validated on load and rejected **as a pair**. Taking
one of the two would produce a plausible-looking span that is silently wrong,
which is worse than falling back to the `config.h` defaults.

## Data flow

```
buttons  → InputManager  → ButtonEvent (incl. LEFT_LONG_PRESS)
         → DisplayUI     → routed by _currentScreen
                         → _commitConfigItem: writes tankFullMm / tankEmptyMm /
                           bypass, sets configDirty
         → ConfigStore   → sees configDirty, writes NVS, clears it
         → SceneEngine   → reads bypass, stands AUTO down
         → RadioReceiver → reads tankFullMm / tankEmptyMm every packet
```

## Changes

### `include/config.h`
- `BUTTON_BACK_HOLD_MS` 2000 — the LEFT hold that returns to home.
- `TANK_MIN_MM` 200 / `TANK_MAX_MM` 4500 — what the editor accepts and what a
  restored NVS value is checked against.
- `CFG_NVS_VER` 1.
- `BOOT_TOTAL_STEPS` 6 → 7 for the new ConfigStore step.

### `lib/InputManager/`
- `ButtonEvent::LEFT_LONG_PRESS`.
- `LONG_MS[]` / `LONG_EVENTS[]` tables; the hold check reads them instead of the
  single `BUTTON_LONG_PRESS_MS` and the `i == BTN_SELECT` gate.

### `lib/SystemState/`
- `bypass` and `configDirty` fields.
- `bypass` joined `StateSnapshot`, the `hasChanged()` chain and `markSeen()`,
  plus `Field::BYPASS`.
- `tankFullMm` / `tankEmptyMm` deliberately **not** in `StateSnapshot`: the config
  screens force their own repaint and `RadioReceiver` reads them directly. Same
  reasoning as the 24h stats. Add them when `CloudClient` needs to publish them.

### `lib/ConfigStore/` — new
`begin()` loads over the defaults with a version guard and a pair validity check;
`update()` writes on the dirty flag. ~85 lines.

### `lib/DisplayUI/DisplayUI.h`
`ScreenId` gained `CONFIG` / `CONFIG_ITEM`; new `ConfigItem`, `ConfigKind`,
`ConfigDef` and the `configDef()` accessor; `_cfgSel` / `_cfgDigit` /
`_cfgBlinkOn` / `_cfgEditMm` / `_cfgEditBool`; `_onHome()` and
`_uiOwnsButtons()`.

### `lib/DisplayUI/DisplayUI.cpp`
- `update()`: `LEFT_LONG_PRESS` handled first, then events routed by screen; the
  draw switch and the redraw gate both gained the config screens.
- `_handleNavigation()`: `RIGHT_PRESS` now leaves home instead of falling into
  `default:`.
- `_applyIdleTimeout()`: off home it times out against `UI_EDIT_TIMEOUT_MS` and
  returns to home discarding — reading a settings list is not a 10 s activity.
- `_getScreenTitle()`: HOME returns `""` rather than `nullptr` (it is a live code
  path now); CONFIG and CONFIG_ITEM return real titles.
- New `_handleConfigList`, `_handleConfigItem`, `_beginConfigItem`,
  `_commitConfigItem`, `_adjustConfigDigit`, `_leaveConfig`.
- `_goTo()` stopped being dead code.

### `lib/DisplayUI/ConfigUI.cpp` — new
Third drawing file in the folder, alongside `HomeUI.cpp` and `BootUI.cpp`. Holds
`CONFIG_DEFS[]`, `_drawConfig`, `_drawConfigItem`, `_drawConfigRow`,
`_drawMetresValue` and the `mmToMetres` formatter.

Layout: list rows font 2, first at y=17, 18 px pitch, selected row on a 0x2945
bar — the title bar's grey, so the highlight reads as furniture rather than a
floating panel. Editor value font 4 centred at y=46, drawn a character at a time
so the cursor digit can blink without the rest of the number moving. Key-map
hints font 1 at y=104/114, because these screens are reached rarely enough that
nobody will remember which button does what.

### `lib/SceneEngine/SceneEngine.cpp`
The bypass guard, below the manual-request consumption.

### `src/main.cpp`
`ConfigStore` instance; `begin()` before `radioReceiver.begin()` (it seeds the
calibration the radio converts through, so it has to be in place before the first
packet); `update()` after `displayUI.update()` so a commit is persisted on the
same pass it is made.

## Known issues / limitations

- `_drawMetresValue()` centres on `textWidth(buf)` but draws per character. If
  TFT_eSPI's per-character advances do not sum to the whole-string width the
  value could sit a pixel or two off centre. Not observed — unverified on
  hardware.
- The list has no scrolling. Six rows fit below the title bar; the seventh
  setting added will need a scroll window.
- `_cfgEditMm` can hold up to 9999 mm mid-edit, which renders fine but is outside
  the accepted range. It is rejected on commit, never stored.

## Not in scope

- **The AUTO logic bypass actually gates.** `SceneEngine` is still a Phase 4 stub,
  so the guard currently short-circuits nothing. It is in place so the AUTO logic
  cannot be written past it later.
- **Manual control and About screens** from Phase 2's original deliverable list.
  The screen strip is built for them; they are not written.
- **Publishing settings to the cloud.** `Field::BYPASS` exists for it; nothing
  consumes it yet.

## Verification

Nothing below has been run — no board was connected. All of it is outstanding.

1. Flash. Boot bar shows 7 steps and the log reads
   `[ConfigStore] no saved config - using defaults` on a device that has never
   saved.
2. RIGHT from home opens CONFIG; LEFT returns. UP/DOWN wrap through the three
   rows and the highlight follows.
3. Hold LEFT 2 s from the list and from the editor — both land on home. Confirm
   the hold never also acts as a short LEFT (the focus must not move twice).
4. SELECT on Tank Full opens an editor titled TANK FULL with a blinking first
   digit. LEFT/RIGHT move the cursor, UP/DOWN cycle 0–9, LEFT off the first digit
   discards, SELECT commits and returns to the list showing the new value.
5. Commit `0.100` (below `TANK_MIN_MM`) and a Full ≥ Empty. Both rejected, cursor
   back on the first digit, reason logged, stored value intact.
6. Toggle Bypass ON. Row reads `ON` in green.
7. Power-cycle. `[ConfigStore] restored - full …mm, empty …mm, bypass …` and the
   rows read back the same values.
8. With the radio node running, set Tank Empty near the current reported distance
   and confirm the home percentage moves, against the
   `[RadioReceiver] seq=.. dist=..mm -> ..%` line.
9. With a config screen open, hold SELECT 1 s — the pump must **not** toggle.
   Back on home, hold SELECT 1 s — it toggles as before.
10. Leave a config screen untouched for 30 s — it returns to home on its own.
11. Regression: timer edit on home still works end to end — focus, edit both rows,
    commit, run.
