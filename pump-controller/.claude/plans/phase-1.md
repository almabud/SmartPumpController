# Phase 1 — Skeleton
## Step-by-step implementation guide

> Goal: the real application architecture compiles, boots, and runs.
> Replace the Adafruit test suite in main.cpp with the actual non-blocking
> scheduler and module stubs. Display shows live home screen. Serial prints
> all module init messages.
>
> Estimated time: 1–2 hours
> Prerequisites: display confirmed working (Phase 0 — done ✓)

---

## What you will have at the end of this phase

```
pump-controller/
├── src/
│   └── main.cpp                  ← real scheduler, replaces test suite
├── include/
│   └── config.h                  ← unchanged
└── lib/
    ├── SystemState/
    │   └── SystemState.h         ← unchanged (already written)
    ├── RadioReceiver/
    │   ├── RadioReceiver.h       ← unchanged
    │   └── RadioReceiver.cpp     ← NEW stub
    ├── PowerMeter/
    │   ├── PowerMeter.h          ← unchanged
    │   └── PowerMeter.cpp        ← NEW stub
    ├── InputManager/
    │   ├── InputManager.h        ← unchanged
    │   └── InputManager.cpp      ← NEW stub
    ├── WifiManager/
    │   ├── WifiManager.h         ← unchanged
    │   └── WifiManager.cpp       ← NEW stub
    ├── CloudClient/
    │   ├── CloudClient.h         ← unchanged
    │   └── CloudClient.cpp       ← NEW stub
    ├── SceneEngine/
    │   ├── SceneEngine.h         ← unchanged
    │   └── SceneEngine.cpp       ← NEW stub
    ├── PumpDriver/
    │   ├── PumpDriver.h          ← unchanged
    │   └── PumpDriver.cpp        ← NEW stub
    └── DisplayUI/
        ├── DisplayUI.h           ← unchanged
        └── DisplayUI.cpp         ← NEW real implementation
```

---

## Step 1 — Create all stub .cpp files

Create one `.cpp` file inside each `lib/` module folder.
These are intentionally minimal — they compile and print their name, nothing else.
Real logic comes in later phases.

### `lib/RadioReceiver/RadioReceiver.cpp`

```cpp
#include "RadioReceiver.h"

void RadioReceiver::begin() {
    Serial.println("[RadioReceiver] begin (stub — Phase 3)");
}

void RadioReceiver::update(SystemState& state) {
    // Phase 3: implement RH_ASK receive, checksum, staleness detection
}
```

---

### `lib/PowerMeter/PowerMeter.cpp`

```cpp
#include "PowerMeter.h"

void PowerMeter::begin() {
    Serial.println("[PowerMeter] begin (stub — Phase 4)");
}

void PowerMeter::update(SystemState& state) {
    // Phase 4: implement ADC sampling + RMS calculation for ZMPT101B and ACS712
}
```

---

### `lib/InputManager/InputManager.cpp`

```cpp
#include "InputManager.h"
#include "config.h"

void InputManager::begin() {
    pinMode(PIN_BTN_UP,     INPUT_PULLUP);
    pinMode(PIN_BTN_DOWN,   INPUT_PULLUP);
    pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
    pinMode(PIN_BTN_BACK,   INPUT_PULLUP);
    Serial.println("[InputManager] begin — 4 buttons ready");
}

void InputManager::update(SystemState& state) {
    // Phase 2: implement debounce (BUTTON_DEBOUNCE_MS) + long-press detection
    _lastEvent = ButtonEvent::NONE;
}
```

---

### `lib/WifiManager/WifiManager.cpp`

```cpp
#include "WifiManager.h"

void WifiManager::begin() {
    Serial.println("[WifiManager] begin (stub — Phase 5)");
}

void WifiManager::update(SystemState& state) {
    // Phase 5: implement non-blocking connect/reconnect with backoff
}
```

---

### `lib/CloudClient/CloudClient.cpp`

```cpp
#include "CloudClient.h"

void CloudClient::begin() {
    Serial.println("[CloudClient] begin (stub — Phase 5)");
}

void CloudClient::update(SystemState& state) {
    // Phase 5: implement MQTT publish/subscribe via PubSubClient
}
```

---

### `lib/SceneEngine/SceneEngine.cpp`

```cpp
#include "SceneEngine.h"

void SceneEngine::update(SystemState& state) {
    // Phase 4: implement hysteresis + AUTO/MANUAL mode logic
    // No action until PowerMeter and RadioReceiver are providing real data
}
```

---

### `lib/PumpDriver/PumpDriver.cpp`

```cpp
#include "PumpDriver.h"
#include "config.h"

void PumpDriver::begin() {
    pinMode(PIN_PUMP_RELAY, OUTPUT);

    // Safe default: pump OFF at boot.
    // Most relay modules are active-LOW (LOW = relay ON, HIGH = relay OFF).
    // If yours is active-HIGH, change HIGH to LOW here and update the
    // comment. Confirm with the relay test in Phase 4.
    digitalWrite(PIN_PUMP_RELAY, HIGH);

    Serial.println("[PumpDriver] begin — pump OFF (relay HIGH)");
}

void PumpDriver::update(SystemState& state) {
    // Phase 4: implement safety rules —
    //   PUMP_MIN_OFF_MS, PUMP_MAX_RUN_MS, stale-data refusal, power-fault refusal
}
```

---

### `lib/DisplayUI/DisplayUI.cpp`

This one is the real implementation for Phase 1 — not a stub.
It shows the live home screen reading from `SystemState`.

```cpp
#include "DisplayUI.h"
#include "config.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

static SPIClass       spiDisplay(FSPI);
static Adafruit_ST7735 tft(&spiDisplay, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

void DisplayUI::begin() {
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);   // backlight on via BC547

    spiDisplay.begin(PIN_TFT_SCLK, -1 /* no MISO */, PIN_TFT_MOSI, PIN_TFT_CS);
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(0);
    tft.fillScreen(ST77XX_BLACK);

    // Boot splash
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(5, 5);
    tft.println("Pump Controller v2");
    tft.setCursor(5, 20);
    tft.setTextColor(ST77XX_GREEN);
    tft.println("Booting...");

    Serial.println("[DisplayUI] begin");
}

void DisplayUI::update(SystemState& state, ButtonEvent event) {
    // Phase 1: home screen only. Menu navigation added in Phase 2.
    _drawHomeScreen(state);
}

void DisplayUI::_drawHomeScreen(SystemState& state) {
    tft.fillScreen(ST77XX_BLACK);

    // --- Row 1: tank level ---
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(5, 5);
    tft.print("Tank: ");
    if (state.tankStale) {
        tft.setTextColor(ST77XX_RED);
        tft.print("-- STALE");
    } else {
        tft.setTextColor(ST77XX_CYAN);
        tft.print(state.tankLevelPct);
        tft.print("%");
    }

    // --- Row 2: pump state + mode ---
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(5, 20);
    tft.print("Pump: ");
    if (state.pumpState == PumpState::ON) {
        tft.setTextColor(ST77XX_GREEN);
        tft.print("ON ");
    } else {
        tft.setTextColor(ST77XX_RED);
        tft.print("OFF");
    }
    tft.setTextColor(ST77XX_WHITE);
    tft.print("  ");
    tft.print(state.mode == OperatingMode::AUTO ? "AUTO" : "MAN ");

    // --- Row 3: voltage + current ---
    tft.setCursor(5, 35);
    tft.print("V:");
    tft.print(state.voltage, 1);
    tft.print("  A:");
    tft.print(state.current, 2);

    // --- Row 4: power + energy ---
    tft.setCursor(5, 50);
    tft.print("W:");
    tft.print(state.powerWatts, 1);
    tft.print("  kWh:");
    tft.print(state.energyKwh, 3);

    // --- Row 5: connectivity ---
    tft.setCursor(5, 65);
    tft.print("WiFi:");
    tft.setTextColor(state.wifiConnected ? ST77XX_GREEN : ST77XX_RED);
    tft.print(state.wifiConnected ? "OK" : "NO");
    tft.setTextColor(ST77XX_WHITE);
    tft.print(" MQTT:");
    tft.setTextColor(state.cloudConnected ? ST77XX_GREEN : ST77XX_RED);
    tft.print(state.cloudConnected ? "OK" : "NO");

    // --- Row 6: fault banner (only shown when there is a fault) ---
    if (state.pumpFault || state.powerFault) {
        tft.setTextColor(ST77XX_BLACK);
        tft.fillRect(0, 80, 128, 14, ST77XX_RED);
        tft.setCursor(5, 83);
        tft.print("!! FAULT DETECTED !!");
    }
}
```

> `_drawHomeScreen` is declared as a private method in `DisplayUI.h`.
> You need to add it to the header — see Step 2.

---

## Step 2 — Update DisplayUI.h

Add the private `_drawHomeScreen` declaration to the existing header.
Open `lib/DisplayUI/DisplayUI.h` and add the private section:

```cpp
// lib/DisplayUI/DisplayUI.h
#pragma once
#include "../SystemState/SystemState.h"
#include "../InputManager/InputManager.h"

class DisplayUI {
public:
    void begin();
    void update(SystemState& state, ButtonEvent event);

private:
    void _drawHomeScreen(SystemState& state);   // ADD THIS LINE
};
```

---

## Step 3 — Write main.cpp

Replace the entire contents of `src/main.cpp` with this.
This is the real scheduler — no `delay()`, every module called at its own cadence.

```cpp
// pump-controller/src/main.cpp
//
// Thin by design. Initialises every module, then runs a non-blocking
// millis()-based scheduler. No delay() anywhere in this file or any module.
// All logic lives in individual modules — this file just wires them together.

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

#include "SystemState/SystemState.h"
#include "RadioReceiver/RadioReceiver.h"
#include "PowerMeter/PowerMeter.h"
#include "InputManager/InputManager.h"
#include "WifiManager/WifiManager.h"
#include "CloudClient/CloudClient.h"
#include "SceneEngine/SceneEngine.h"
#include "PumpDriver/PumpDriver.h"
#include "DisplayUI/DisplayUI.h"

// ---- Central state -------------------------------------------------------
SystemState state;

// ---- Module instances ----------------------------------------------------
RadioReceiver  radioReceiver;
PowerMeter     powerMeter;
InputManager   inputManager;
WifiManager    wifiManager;
CloudClient    cloudClient;
SceneEngine    sceneEngine;
PumpDriver     pumpDriver;
DisplayUI      displayUI;

// ---- Scheduler timestamps ------------------------------------------------
uint32_t lastRadioMs   = 0;
uint32_t lastPowerMs   = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastWifiMs    = 0;
uint32_t lastCloudMs   = 0;
uint32_t lastSceneMs   = 0;

// -------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);   // settle time for serial monitor — boot only, not in loop
    Serial.println("=== pump-controller v2 booting ===");

    radioReceiver.begin();
    powerMeter.begin();
    inputManager.begin();
    wifiManager.begin();
    cloudClient.begin();
    pumpDriver.begin();
    displayUI.begin();

    Serial.println("=== all modules initialised ===");
}

void loop() {
    uint32_t now = millis();

    // --- Every iteration: time-sensitive modules --------------------------
    inputManager.update(state);     // buttons need fast polling
    pumpDriver.update(state);       // safety timers must stay sharp

    // --- Radio: fast poll -------------------------------------------------
    if (now - lastRadioMs >= INTERVAL_RADIO_MS) {
        radioReceiver.update(state);
        lastRadioMs = now;
    }

    // --- Power meter: 1 second --------------------------------------------
    if (now - lastPowerMs >= INTERVAL_POWER_MS) {
        powerMeter.update(state);
        lastPowerMs = now;
    }

    // --- Wi-Fi health check -----------------------------------------------
    if (now - lastWifiMs >= INTERVAL_WIFI_CHECK_MS) {
        wifiManager.update(state);
        lastWifiMs = now;
    }

    // --- MQTT publish / receive -------------------------------------------
    if (now - lastCloudMs >= INTERVAL_CLOUD_MS) {
        cloudClient.update(state);
        lastCloudMs = now;
    }

    // --- Scene engine: decision layer -------------------------------------
    if (now - lastSceneMs >= 200) {
        sceneEngine.update(state);
        lastSceneMs = now;
    }

    // --- Display: slow refresh --------------------------------------------
    if (now - lastDisplayMs >= INTERVAL_DISPLAY_MS) {
        displayUI.update(state, inputManager.lastEvent());
        lastDisplayMs = now;
    }
}
```

---

## Step 4 — Build

From inside `pump-controller/`:

```bash
pio run
```

**Expected result:** zero errors, zero warnings (maybe a few "unused variable"
notes from stub functions — those are fine and will go away as phases are implemented).

If you get errors, the most common causes are:

| Error message | Likely cause | Fix |
|---|---|---|
| `cannot open source file "xxx.h"` | Missing `#include` or wrong path | Check the include path matches the folder name exactly |
| `undefined reference to xxx::begin()` | `.cpp` file not created or wrong folder | Confirm the `.cpp` exists inside the module's `lib/` subfolder |
| `no member named '_drawHomeScreen'` | `DisplayUI.h` not updated | Add the private declaration (Step 2) |
| `'PIN_BTN_UP' was not declared` | `config.h` not included in `.cpp` | Add `#include "config.h"` at the top of `InputManager.cpp` |

---

## Step 5 — Upload and verify serial output

```bash
pio run -t upload
pio device monitor
```

**Expected serial output — exactly this, in this order:**

```
=== pump-controller v2 booting ===
[RadioReceiver] begin (stub — Phase 3)
[PowerMeter] begin (stub — Phase 4)
[InputManager] begin — 4 buttons ready
[WifiManager] begin (stub — Phase 5)
[CloudClient] begin (stub — Phase 5)
[PumpDriver] begin — pump OFF (relay HIGH)
[DisplayUI] begin
=== all modules initialised ===
```

If you don't see all lines, something didn't initialise — check which line
is missing and look at the corresponding `.cpp` file.

---

## Step 6 — Verify the display

**Expected display output:**

```
Tank: -- STALE
Pump: OFF  AUTO
V:0.0  A:0.00
W:0.0  kWh:0.000
WiFi:NO MQTT:NO
```

All zeros and STALE are correct at this stage — no sensors are connected yet.
The display should refresh every 500ms (INTERVAL_DISPLAY_MS).

**If the display is blank:** backlight circuit issue — check BC547 wiring and
confirm GPIO21 is being driven HIGH in `DisplayUI::begin()`.

**If the display shows garbled content:** check that `INITR_BLACKTAB` and
`setRotation(0)` match what worked in the display test earlier.

---

## Step 7 — Confirm the relay is safe

Look at (or listen to) your relay module. It should **not** click at boot.
`PumpDriver::begin()` drives `PIN_PUMP_RELAY` HIGH, which should keep a
standard active-LOW relay de-energized.

If the relay clicks ON at boot, your module is active-HIGH — change
`PumpDriver.cpp` line:
```cpp
digitalWrite(PIN_PUMP_RELAY, HIGH);   // change HIGH → LOW
```
And update the comment. Note which logic your relay uses — it matters for Phase 4.

---

## Phase 1 checkpoint — done when all of these pass

- [ ] `pio run` compiles with zero errors
- [ ] Serial monitor shows all 9 init messages in the correct order
- [ ] Display shows the home screen with correct layout (all zeros, STALE, WiFi: NO)
- [ ] Display refreshes visibly (watch the screen — it redraws every 500ms)
- [ ] Relay does NOT click at boot
- [ ] Board runs for 5 minutes without crashing or rebooting
- [ ] No `delay()` calls exist anywhere in the new code (grep check below)

**Final check — confirm no delay() in the application code:**

```bash
grep -r "delay(" src/ lib/ --include="*.cpp" --include="*.h"
```

The only acceptable `delay()` is the single `delay(300)` in `setup()` for
serial monitor settle time. If you see any `delay()` inside `loop()` or inside
any module's `update()` function, remove it before calling Phase 1 done.

---

## Next step

Phase 1 done → move to **Phase 2: Local inputs**
(InputManager debounce + DisplayUI menu navigation)