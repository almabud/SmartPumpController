# Boot screen — branded splash with init progress

> Status: **implemented** — builds clean; not yet confirmed on hardware.

## Context

`setup()` (`src/main.cpp:21-30`) currently boots straight into the home screen:
it prints two lines to serial and calls `inputManager.begin()` /
`displayUI.begin()`. On the device this reads as a black screen that abruptly
becomes the dashboard — no branding, and no clue where a hang occurred once the
six stub modules (Radio, Power, Pump, Wi-Fi, Cloud, Scene) start doing real
work in Phase 3.

This adds a boot screen: droplet icon, two-line product title, a progress bar
driven by actual module init, and a status line naming the module currently
starting. Each `begin()` advances the bar, with a per-step minimum dwell so the
fill is visible even though init today takes milliseconds.

## Layout — 160x128 landscape

```
┌──────────────────────────┐
 y6            ●             droplet , 9x13, TFT_CYAN, centered x=80
 y26        SMART            font 4, TFT_CYAN, centered —  84 px
 y58   PUMP CONTROLLER       font 2, TFT_CYAN, centered — 118 px
 y92  ▓▓▓▓▓▓▓▓░░░░░░░░       bar: x=20 w=120 h=6, grey outline, cyan fill
 y104   Starting inputs...   font 1, TFT_DARKGREY, centered
 y118                 v2.0   font 1, TFT_DARKGREY, right-aligned
└──────────────────────────┘
```

Vertical budget: 6+13 icon, 26..52 hero (font 4 is 26 px), 58..74 name (font 2
is 16 px), 92..98 bar, 104..112 status, 118..126 version. Fits 128 with no
overlap.

**Why the title is split across two fonts.** Font 4 cannot hold the full
product name. Measured against TFT_eSPI's own width tables
(`.pio/libdeps/esp32-s3/TFT_eSPI/Fonts/Font32rle.c`, `Font16.c`):

| String | Font 4 | Font 2 |
|---|---|---|
| `SMART PUMP` | **160 px** | 82 px |
| `CONTROLLER` | **164 px** | 78 px |
| `SMART` | 84 px | — |
| `PUMP CONTROLLER` | — | 118 px |

The screen is 160 px wide, so both of the originally planned font-4 lines
clip. Fonts 6 and 7 are digits-only, so font 4 is the largest face available —
hence one font-4 hero word over the full name in font 2.

## Font facts (verified, not assumed)

Parsed the bundled VLW at `include/FontAwesomesolid9006.h`: **492 glyphs,
U+F013..U+F1EB, 125 gaps inside that range**, fontSize 12, ascent 10.
Confirmed present and usable here: droplet `` (9x13), bolt ``,
circle-check ``, gear ``, wifi ``, cloud ``.
Absent: thermometer, faucet, power-off, plain check.

Font 4 is enabled (`-D LOAD_FONT4`) and is 26 px tall, the largest readable
face available. Its width table is the reason for the title split above.
Pulling font 4 in for the first time cost ~81 KB of flash (427 KB → 508 KB,
7.8 % of 6.5 MB) — plenty of headroom, but worth knowing it is not free.

## Changes

### 1. `include/config.h` — new boot constants

```c
#define FIRMWARE_VERSION    "v2.0"
#define BOOT_TOTAL_STEPS    3       // Display, Inputs, Ready — bump as modules land
#define BOOT_STEP_MIN_MS    250     // per-step dwell so the bar visibly fills
```

### 2. `lib/DisplayUI/DisplayUI.h` — new public API

```cpp
// Boot screen — called from setup() only, before the scheduler starts.
void showBoot(uint8_t step, uint8_t total, const char* label);
```
plus a private `void _drawBoot(uint8_t step, uint8_t total, const char* label);`

`showBoot()` draws and pushes the sprite immediately. It deliberately does
**not** go through `update()`: that path is gated on
`state.hasChanged(Consumer::DISPLAY_CONSUMER)` (`DisplayUI.cpp:24`) and needs a
`SystemState`, neither of which applies during boot. Boot is therefore *not*
added to `ScreenId` — it is a setup-time sequence, not a screen the navigation
state machine can reach.

### 3. `lib/DisplayUI/BootUI.cpp` — new file

Mirrors the existing `HomeUI.cpp` split (one file per screen, `extern
TFT_eSprite _sprite;` at the top, function-local `const` geometry). Structure:

- `showBoot()` — calls `_drawBoot()`, `_sprite.pushSprite(0, 0)`, then
  `delay(BOOT_STEP_MIN_MS)`.
- `_drawBoot()` — `fillSprite(TFT_BLACK)`, then icon → title → bar → status →
  version.

Implementation notes:

- Center the title with `setTextDatum(TC_DATUM)` + `drawString(..., 80, y, 4)`
  rather than the manual `setCursor` arithmetic used elsewhere — the strings
  are fixed but proportional-font widths are not worth hardcoding. Reset to
  `TL_DATUM` before returning, since the rest of the codebase assumes it.
- Icon follows the `_drawCloudIcon()` pattern (`HomeUI.cpp:50-58`):
  `loadFont(FontAwesomesolid9006)` → `drawString("", ...)` →
  `unloadFont()`.
- Bar: `drawRect(20, 92, 120, 6, TFT_DARKGREY)` outline, then
  `fillRect(21, 93, (118 * step) / total, 4, TFT_CYAN)`. Guard `step > total`.
- Restore `setTextFont(1)` on exit — the convention every function in
  `HomeUI.cpp` follows.

### 4. `src/main.cpp` — sequence the boot

```cpp
displayUI.begin();                                   // display first — nothing
displayUI.showBoot(1, BOOT_TOTAL_STEPS, "Display");  // can be shown before it

inputManager.begin();
displayUI.showBoot(2, BOOT_TOTAL_STEPS, "Inputs");

displayUI.showBoot(BOOT_TOTAL_STEPS, BOOT_TOTAL_STEPS, "Ready");
```

Note the reordering: `inputManager.begin()` currently runs first
(`main.cpp:26`), but the display has to be alive before any step can be drawn.

`delay()` inside `showBoot()` is confined to `setup()`, where `main.cpp:23`
already has a `delay(300)`. The "no `delay()`" rule from `milestones.md`
applies to `loop()`, which is untouched.

As Phase 3 modules land, each gets a `begin()` + `showBoot(n, ...)` pair and
`BOOT_TOTAL_STEPS` goes up.

## Verification

1. `pio run` — must compile clean; `BootUI.cpp` should appear in the build log
   alongside `HomeUI.cpp`.
2. `pio run -t upload && pio device monitor` — watch the screen through boot.
3. Expect: splash appears almost immediately after backlight on, bar fills in
   three visible increments over ~750 ms, status reads Display → Inputs →
   Ready, then the home screen replaces it on the first 500 ms display tick.
4. Check both title lines have margin on either side — widths were computed
   from the font tables, not eyeballed, so this is a sanity check rather than a
   likely failure.
5. Confirm the home screen still draws correctly afterwards, i.e. the datum and
   font were reset (a stuck `TC_DATUM` or font 4 would visibly wreck the tank
   column).
