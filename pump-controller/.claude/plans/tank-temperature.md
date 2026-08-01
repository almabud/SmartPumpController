# Tank temperature — state field + home UI readout

> Status: **done** — shipped in `7ec4a80` on `feat/wifi`.

## Context

The remote Nano tank sensor reads a DS18B20 for speed-of-sound correction, but
that temperature was invisible on the controller — it existed neither in
`SystemState` nor on screen. This change surfaces the water tank temperature as
a first-class reading: stored in the single source of truth (so it participates
in change detection and can later be filled by the 433 MHz packet), and
rendered on the home screen inside the tank column, under the level percentage.

Outcome: the tank column shows `75%` on one line and `27.5°C` on the line
below, both hidden while `tankStale` is true, matching the existing level
readout.

## Decisions

- Storage: `float tankTempC`, one decimal on screen, gated by the existing
  `tankStale` flag (no separate valid flag).
- Placement: inside the tank bar, under the `%` text — `_drawTankTemp()` is
  called from `_drawHome()` right after `_drawTankLevel()`.

## Changes

### `lib/SystemState/SystemState.h`

- Tank block: `float tankTempC = 27.5f;` — placeholder default until the radio
  writes it.
- `enum class Field`: added `TANK_TEMP` after `TANK_STALE`.
- `struct StateSnapshot`: added `float tankTempC = -1.0f;` — the `-1` sentinel
  convention used by the other floats.

### `lib/SystemState/SystemState.cpp`

All three functions updated together, per the existing pattern:

- `hasChanged(Consumer)` — `floatChanged(tankTempC, s.tankTempC)` added to the
  float group.
- `hasChanged(Consumer, Field)` — `case Field::TANK_TEMP:` after `TANK_STALE`.
- `markSeen(Consumer)` — `s.tankTempC = tankTempC;`.

### `lib/DisplayUI/DisplayUI.h`

`void _drawTankTemp(SystemState& state);` declared next to `_drawTankLevel`.

### `lib/DisplayUI/HomeUI.cpp`

New `_drawTankTemp()` after `_drawTankLevel()`, plus the call site in
`_drawHome()`.

Geometry: the tank inner area is `x 3..40` (38 px wide), outline `y 16..125`,
and the `%` text is font 2 (16 px tall) at `y 64`. Temperature sits at `y 84`
and uses **font 1 (6x8)** — font 2 at ~8 px/char would overflow 38 px.

Notes that drove the implementation:

- **No degree glyph exists.** TFT_eSPI's Font1/Font2 cover ASCII 32-127 only,
  and the bundled `FontAwesomesolid9006` VLW covers only U+F013..U+F1EB (no
  thermometer either). The degree mark is drawn as a primitive —
  `drawCircle(x, y + 1, 1, TFT_WHITE)` — consistent with every other widget in
  this file.
- Centered on the tank axis `x = 22` via `textWidth()` rather than the
  digit-count `if/else` ladder used for the percentage, since temperature width
  varies more.
- Text is transparent (single-arg `setTextColor`) so the tank fill shows
  behind it. White reads fine on the blue/orange/red fill and on black.
- Returns early when `state.tankStale`, mirroring `_drawTankLevel`.

```cpp
void DisplayUI::_drawTankTemp(SystemState& state) {
    if (state.tankStale) return;

    const int16_t CENTER_X = 22;   // BAR_X + BAR_WIDTH / 2
    const int16_t Y        = 84;   // just below the level % (font 2 at y = 64)

    char buf[8];
    snprintf(buf, sizeof(buf), "%.1f", state.tankTempC);

    _sprite.setTextFont(1);
    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_WHITE);

    int16_t valueWidth = _sprite.textWidth(buf);
    int16_t totalWidth = valueWidth + 3 + _sprite.textWidth("C");
    int16_t x          = CENTER_X - totalWidth / 2;

    _sprite.setCursor(x, Y);
    _sprite.print(buf);
    _sprite.drawCircle(x + valueWidth + 1, Y + 1, 1, TFT_WHITE);
    _sprite.setCursor(x + valueWidth + 4, Y);
    _sprite.print("C");
}
```

## Out of scope — still open

- `RadioReceiver::update()` (`lib/RadioReceiver/RadioReceiver.cpp:7`) is a
  Phase-3 stub, so nothing writes `tankTempC` yet; the default is what shows.
- No `SensorPacket` struct exists anywhere — carrying temperature over 433 MHz
  is a separate task, and note that `milestones.md` originally specified the
  DS18B20 reading as a Nano-side compensation input, "not displayed alone".
  Displaying it is a deliberate departure from that.
- A `-15.5C` reading measures 39 px and would clip the 38 px inner bar by 1 px.
  Not a realistic water temperature, so it was left alone.

## Verification

1. `pio run` — compiles clean (watch that the `Field` switch and the snapshot
   struct stay in sync). Confirmed: RAM 6.1%, Flash 6.5%.
2. `pio run -t upload && pio device monitor` — flash and eyeball the screen.
3. Defaults are `tankStale = false`, `tankLevelPct = 50`, `tankTempC = 27.5f`,
   so the tank column should read `50%` over `27.5°C`, centered with no
   clipping at either edge.
4. Poke values in `src/main.cpp` for the edge cases: `-5.0f` (widest string),
   `100.0f` (3 digits + decimal), and `tankStale = true` (both readouts vanish,
   outline stays).
5. Set `tankLevelPct = 90` to check text contrast against the blue fill.
