# Bypass status icon and the LEFT-hold shortcut

> Status: **implemented** (`fdde20c..`). Builds clean. **No hardware
> verification** — no board was connected, so everything under Verification
> below is outstanding, as is the whole of [config-page.md](config-page.md)
> that this sits on.

## Context

[config-page.md](config-page.md) shipped a `Bypass` row that stands the AUTO
level logic down in `SceneEngine::update()`. Two gaps remain:

1. **Bypass is invisible from the home screen.** It changes how the pump
   behaves, and the only way to see it is to navigate two screens away. A mode
   you can forget you armed is the kind that bites.
2. **Arming it takes five presses** — RIGHT, DOWN, DOWN, SELECT, UP, SELECT.
   Too slow for what is effectively the override you reach for when something is
   already wrong.

This adds a title-bar status icon and a hold-to-toggle shortcut on the home
screen. The resulting key map across all screens is documented in
[ui_guide.md](../../docs/ui_guide.md).

**This builds on code that has never run on hardware.** The config page compiles
but was committed with no board attached, so every check in that plan is still
outstanding. Verification here subsumes it.

## Decisions

### LEFT's hold means different things on different screens

The two actions never overlap, so `_currentScreen` is the whole conflict
resolution:

| Screen | LEFT held |
|---|---|
| `HOME` | toggle bypass — 3 s |
| `CONFIG`, `CONFIG_ITEM` | back to home — 2 s |

The **durations** differ, which is the only part that is not free: `InputManager`
picks the threshold and has no idea which screen is up. It gets told, through the
same channel `state.uiEditing` already is — a new `state.uiOnHome` that
`DisplayUI` writes and `InputManager` reads.

Different weights are deliberate. Returning home is harmless navigation and
should be quick; arming bypass changes how the pump behaves and should take a
hold nobody does by accident.

**Mid-hold screen changes are safe, and this is worth being explicit about.**
Hold LEFT on `CONFIG`: at 2 s the event fires, `DisplayUI` goes home, and
`uiOnHome` flips true — so the threshold becomes 3000 while the button is still
down. `longFired` is already set by then, so nothing fires a second time. No
guard needed; the latch that suppresses the short press on release does this job
too.

**One loop pass of lag.** `InputManager::update()` runs before
`displayUI.update()` in `loop()`, so it reads the previous pass's `uiOnHome`. At
loop rates that is microseconds, and a screen change resets nothing about an
in-flight hold, so the stale read cannot change an outcome.

### The shortcut toggles, and stands down mid-edit

Each hold flips the flag; the config page row stays as the other way in. A
shortcut that could only arm bypass would leave the cure two screens away from
an accidental trigger.

Guarded by `!_editing()`: on home with the timer editor open, the buttons belong
to that editor, which uses LEFT to walk backwards through its fields. The
shortcut stands down rather than reaching past it — the same reasoning that makes
`InputManager` stand down on `uiEditing`.

The toggle sets `state.configDirty`, so `ConfigStore` persists it on the same
loop pass. Reaching for the shortcut and losing the setting on the next power
cycle would be worse than not having the shortcut.

### Icon: ban U+F05E, always present, dim when off

Verified present in the bundled VLW at 12x13 with `dY=11`. **No font
regeneration is needed** — the whole U+F013–U+F1EB block is already baked in
(348 unique glyphs, 80 KB of PROGMEM), because TFT_eSPI's `Create_font.pde`
reads its `unicodeBlocks` array as start/end *pairs* rather than as individual
codepoints. [custom_font_generator.md](../../docs/custom_font_generator.md) §5
presents it as a list of four hand-picked icons, which is wrong and gets
corrected as part of this work.

A circle-with-slash reads as "automation suppressed" without a legend. Dim grey
when off, red when on — the always-present pattern `_drawCloudIcon()` uses, so
the bar never reflows and the icon's presence teaches that the feature exists.

### Geometry

The gap to the right of the cloud is only ~5 px, so the icon goes to its left:

```
┌──────────────────────────────────────┐
│ CONFIG            ⊘   ☁  ●  ▁▃▅▇    │  y0-13
└──────────────────────────────────────┘
  x=4              100  118 136 147
  title            ban  cloud hb  bars
```

Ban is 12 wide → 100..111, leaving 6 px to the cloud at 118. That is wider than
the ~4 px between the other three icons on purpose: ban's ink fills its box edge
to edge while the cloud's does not, so equal numbers read as unequal gaps.

Title text keeps x=4..98 — about 15 characters of font 1, and the longest title
in the app (`TANK EMPTY`) is 10.

`_drawTitleBar()` is shared, so the icon appears on every screen rather than only
home. That is correct: bypass is global state.

## Data flow

```
LEFT held 3s on home
  → InputManager  emits LEFT_LONG_PRESS (threshold picked from state.uiOnHome)
  → DisplayUI     _toggleBypass(): flips state.bypass, sets state.configDirty
  → ConfigStore   sees configDirty, writes NVS, clears it
  → SceneEngine   reads state.bypass, stands AUTO down
  → DisplayUI     bypass is in StateSnapshot, so the flip forces a repaint
                  _drawBypassIcon() renders it red
```

## Changes

### `include/config.h`
`BUTTON_BYPASS_HOLD_MS` 3000, next to the existing `BUTTON_BACK_HOLD_MS`.

### `lib/SystemState/SystemState.h`
`uiOnHome`, defaulting true (the UI boots on home), beside `uiEditing` which it
mirrors. **Not** added to `StateSnapshot` — it is a UI-to-input channel, not
displayed state, exactly as `uiEditing` is handled.

### `lib/InputManager/InputManager.cpp`
`LONG_MS[]` stays as the table of defaults; LEFT's entry is overridden per pass
inside the button loop, just above the hold check. No new event and no staged
hold — `LEFT_LONG_PRESS` already exists and carries both meanings.

### `lib/DisplayUI/DisplayUI.{h,cpp}`
- The `LEFT_LONG_PRESS` branch in `update()` gains the home/elsewhere split and
  the `!_editing()` guard. It no longer drops focus on home — this is a global
  action, not navigation.
- `state.uiOnHome = _onHome();` beside the existing `uiEditing` write.
- New `_toggleBypass()`. Deliberately not folded into `_commitConfigItem()`,
  which is driven by `_cfgSel` and the editor's working copies.

### `lib/DisplayUI/HomeUI.cpp`
New `_drawBypassIcon()` next to `_drawCloudIcon()`, same load/draw/unload shape,
called from `_drawTitleBar()`.

Drawn at `Y = 0` rather than the cloud's `Y = 1`: smooth-font glyphs are placed
at `y + (maxAscent - dY)`, and ban's `dY` is 11 against the cloud's 10, so it
sits one row lower for the same `Y`. Starting a row higher lands its 13 px of ink
in rows 1..13, inside the bar.

### `docs/custom_font_generator.md`
§5 correction — `unicodeBlocks` is start/end pairs, and any icon in
U+F013–U+F1EB is already available without regenerating.

## Known issues / limitations

- **The icon's vertical placement is arithmetic, not observation.** The
  `maxAscent - dY` reasoning above predicts rows 1..13, but smooth-font datum
  handling is fiddly. It has to be eyeballed against the y=13 separator.
- **A 3 s hold has no progress feedback.** The icon changing at the end is the
  only signal that anything happened. The project already accepts blind holds
  (SELECT 1 s toggles the pump the same way), so this is consistent rather than
  good.
- The 2 s hold on home does nothing at all now. Previously it dropped focus.

## Not in scope

- **What bypass actually gates.** `SceneEngine` is still a Phase 4 stub, so the
  guard short-circuits nothing yet.
- A hold-progress indicator.
- Trimming the font. 80 KB of PROGMEM for 348 glyphs when 6 are used is the
  bigger win, and is unrelated to this change.

## Stages

Each compiles clean and is verifiable on its own.

1. **The plan doc** — this file, cross-linked from `config-page.md`. No code.
   `docs: Plan the bypass status icon and LEFT-hold shortcut`
2. **Behaviour** — `config.h`, `SystemState.h`, `InputManager.cpp`,
   `DisplayUI.{h,cpp}`. No icon yet; the config page's Bypass row is the readout.
   `feat: Toggle bypass with a LEFT hold on the home screen`
3. **The icon** — `HomeUI.cpp`, `DisplayUI.h`.
   `feat: Show bypass state in the title bar`
4. **Close out** — the font generator doc correction, and this file's status
   header flipped to what was actually verified.
   `docs: Correct the font generator's Unicode list`

## Verification

Hardware. Nothing in the config-page commit has run on a board either, so run
that plan's checklist first — this sits on top of it.

1. `/build` clean, then `/flash`.
2. **Shortcut:** on home, hold LEFT 3 s. Icon goes red, serial logs it. Hold
   again → back to grey. A 2 s hold on home does nothing at all.
3. **No collision:** RIGHT to config, hold LEFT 2 s → lands on home, bypass
   unchanged. Confirm the hold does not *also* toggle bypass on arrival.
4. **Mid-hold crossing:** from config, hold LEFT well past 3 s. It should go home
   at 2 s and then do nothing further, however long the hold continues.
5. **Editor guard:** on home, focus the timer, SELECT to edit, hold LEFT. Bypass
   must not move; the field cursor behaves as before.
6. **Icon placement:** no clipping at the top of the bar or against the y=13
   separator, no overlap with the cloud at x=118. Check on `CONFIG_ITEM` with
   `TANK EMPTY` as the title.
7. **Persistence:** toggle via the shortcut, power-cycle, confirm
   `[ConfigStore] restored - ... bypass on` and a red icon at boot.
8. **Both routes agree:** flip on the config page, return home, icon matches.
   Flip via the shortcut, open the config page, row matches.
9. **Regression:** short LEFT on home still drops focus; SELECT held 1 s still
   toggles the pump on home and still does not from a config screen.
