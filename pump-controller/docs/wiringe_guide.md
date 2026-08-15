# Wiring Reference — pump-controller (ESP32-S3-WROOM-1 N16R8)

> This is the permanent wiring reference for the main controller board.
> All pin numbers match `pump-controller/include/config.h` exactly.
> **Never modify wiring without updating config.h and this document together.**
>
> Board: ESP32-S3-WROOM-1 N16R8 Dual Type-C
> Always use the **USB port** (not COM port) for uploading and monitoring.

---

## Power rails

| Source | Voltage | Notes |
|--------|---------|-------|
| USB-C adapter | 5V input | Use 5V/2A minimum adapter |
| ESP32 VIN pin | 5V output | Bridged jumper (IN/OUT soldered) — confirmed 4.88V |
| ESP32 3V3 pin | 3.3V output | For 3.3V-only modules |
| Common GND | 0V | All modules share this rail |

---

## 1. TFT Display (ST7735S 1.8" 128×160)

**Bus:** SPI2 / FSPI — dedicated, display only.
**Backlight:** BC547 NPN transistor low-side switch (see circuit below).

| Display Pin | Connects to | Notes |
|-------------|-------------|-------|
| VCC | 3.3V or 5V | Check your module's rating |
| GND | GND | |
| CLK / SCK | GPIO40 | SPI2 SCLK |
| SDA / DIN / MOSI | GPIO41 | SPI2 MOSI |
| CS | GPIO39 | SPI2 CS |
| RS / DC / A0 | GPIO38 | Data/Command select |
| RST / RESET | GPIO47 | Reset |
| LED+ | 5V (VIN) | Backlight positive — direct to 5V |
| LED- | BC547 Collector | Backlight negative — through transistor |

**Backlight driver circuit (BC547 NPN low-side switch):**

```
5V ────────────────── LED+
LED- ──────────────── BC547 Collector
BC547 Emitter ─────── GND
GPIO21 ──[2kΩ]──────── BC547 Base
```

BC547 pinout (flat side facing you, legs down): Left=C, Middle=B, Right=E

**config.h pins:**
```cpp
#define PIN_TFT_SCLK   40
#define PIN_TFT_MOSI   41
#define PIN_TFT_CS     39
#define PIN_TFT_DC     38
#define PIN_TFT_RST    47
#define PIN_TFT_BL     21
```

---

## 2. Push Buttons (×5, active-low)

No external resistors needed — uses ESP32 internal INPUT_PULLUP.
Wire one leg to GPIO, other leg to GND. No polarity.

| Button | GPIO | Role |
|--------|------|------|
| Button 1 | GPIO11 | LEFT |
| Button 2 | GPIO12 | UP |
| Button 3 | GPIO13 | SELECT / OK (confirm) |
| Button 4 | GPIO14 | RIGHT |
| Button 5 | GPIO6 | DOWN |

**Logic:** pressed = LOW, released = HIGH (active-low).

**config.h pins:**
```cpp
#define PIN_BTN_LEFT    11
#define PIN_BTN_UP      12
#define PIN_BTN_SELECT  13
#define PIN_BTN_RIGHT   14
#define PIN_BTN_DOWN     6
```

---

## 3. 433 MHz ASK Receiver

**Note:** Receiver DATA pin outputs 5V logic — voltage divider required before GPIO16.

| Module Pin | Connects to | Notes |
|------------|-------------|-------|
| VCC | 5V (VIN) | |
| GND | GND | |
| DATA | 2.1kΩ → GPIO16, then 4.6kΩ → GND | Voltage divider — see circuit |
| ANT | 17.3cm straight wire | Mandatory for range |

**Voltage divider circuit:**

```
Module DATA ──[2.1kΩ]──┬── GPIO16
                        │
                     [4.6kΩ]
                        │
                       GND
```

Divider output: 5V × 4.6/(2.1+4.6) = ~3.43V — within ESP32 ADC safe range.

**config.h pin:**
```cpp
#define PIN_RF433_RX   16
```

---

## 4. Current Sensor (ACS712-30A)

**Note:** ACS712 runs on 5V, output can reach ~4.5V — voltage divider required before GPIO4.

| Module Pin | Connects to | Notes |
|------------|-------------|-------|
| VCC | 5V (VIN) | |
| GND | GND | |
| OUT | 2.1kΩ → GPIO4, then 4.6kΩ → GND | Voltage divider |
| IP+ | Live wire FROM source | Mains side — wire last |
| IP- | Live wire TO relay COM | Mains side — wire last |

**Voltage divider circuit (same ratio as ASK receiver):**

```
ACS712 OUT ──[2.1kΩ]──┬── GPIO4
                       │
                    [4.6kΩ]
                       │
                      GND
```

**Calibration constants in config.h:**
```cpp
#define PIN_CURRENT_SENSE      4
#define ACS712_DIVIDER_RATIO   0.686f   // 4.6/(2.1+4.6)
#define ACS712_MV_PER_AMP      66.0f    // 30A variant
#define POWER_NOISE_FLOOR_A    0.15f    // below this, report a clean 0
```

After the divider the part gives `66 × 0.686 = 45.3 mV` per amp at the pin, so a
12-bit ADC over the 0–3.3 V range resolves roughly 56 counts per amp. Fine for
pump-scale currents, useless for anything under a few hundred milliamps — hence
the noise floor. See "Calibrating the power sensors" below.

---

## 5. Voltage Sensor (ZMPT101B)

**Note:** Powered from 5V, no divider. The trim pot is what keeps the output
inside the ADC's range — see the headroom warning below.

| Module Pin | Connects to | Notes |
|------------|-------------|-------|
| VCC | 5V | Biases the output near 2350mV — the pot must compensate |
| GND | GND | |
| OUT | GPIO5 | Direct connection, no divider |
| AC Terminal 1 | Live (mains) | Parallel connection — wire last |
| AC Terminal 2 | Neutral (mains) | Parallel connection — wire last |

**Asymmetric headroom — this is the thing to watch.** On 5V the module idles at
roughly `2350 mV` (measured on this board), while the ADC's usable ceiling is
about `3000 mV`. That leaves only **~650 mV of room above the bias** against
~2250 mV below it, so the positive half-cycle is what hits the limit first.

Set the trim pot so the peak stays inside that 650 mV. Anything beyond it is
clipped by the ADC, and **clipping is silent**: the waveform is flattened, the
RMS reads low, and nothing about the number looks wrong. Calibrating against a
clipped reading bakes the error in permanently.

The firmware watches for this. With `POWER_CAL_LOG` on it prints a `range` line
each second giving the raw min/max and the remaining headroom, and it prints a
`CLIPPING` warning — regardless of `POWER_CAL_LOG` — whenever samples land
outside `ADC_CLIP_LOW_MV .. ADC_CLIP_HIGH_MV`. Tune the pot by watching `head`
rather than by guessing; leave a few hundred mV of margin so mains fluctuation
does not push it over.

Resolution is not a concern at this bias: ~650 mV of usable swing is about 800
ADC counts of peak, which resolves 230 V mains to well under a volt.

> If you would rather have symmetric headroom, the alternatives are to add the
> same 2.1k/4.6k divider used on the ACS712 (bias drops to ~1610 mV, needs no
> firmware change since `ZMPT_CAL_V_PER_V` is measured end-to-end), or to move
> VCC to 3.3V (bias ~1650 mV — but check the op-amp marking first; an LM358 is
> not rail-to-rail and its swing gets cramped on a 3.3V single supply).

**config.h pin and calibration:**
```cpp
#define PIN_VOLTAGE_SENSE   5
#define ZMPT_CAL_V_PER_V    220.0f   // PLACEHOLDER — must be measured, see below
```

`ZMPT_CAL_V_PER_V` is mains volts per volt seen at GPIO5. Because the trim pot
sets the module's gain, **no shipped default can be correct** — the value is a
property of how far that pot happens to be turned on this particular board.
Every voltage, watt and kWh figure the firmware produces is wrong until it is
measured. See "Calibrating the power sensors" below.

---

## 6. Relay Module (JQC-3F-05VDC-C)

**Control side (signal — low voltage):**

| Module Pin | Connects to | Notes |
|------------|-------------|-------|
| VCC | 5V (VIN) | Powers the relay coil |
| GND | GND | Must be common with the ESP32 |
| IN | BC547 Collector | **Not** direct from GPIO18 — see driver circuit below |

**Why a driver is needed:** the IN pin is **current-driven, not voltage-driven**.
The relay energises whenever current can flow out of IN, so what matters is
whether IN has a path to ground — not what voltage sits on it.

Measured on this module:

| IN pin | Reads | Relay |
|--------|-------|-------|
| Disconnected (open) | 0.6V | OFF |
| Tied to GND | 0V | ON |
| Driven by GPIO18 at 3.3V | 3.3V | **ON** — the bug |

An ESP32 output at HIGH is a low-impedance sink, so it still passes enough
current to hold the relay on. Counter-intuitively a floating pin turns it off
while a 3.3V pin does not. IN never needs to reach 5V — it needs to be left
**open**, which no directly-wired GPIO can ever do.

This module has no VCC/JD-VCC jumper, so the split-rail fix is unavailable.

**Relay driver circuit (BC547 NPN low-side switch — same as the backlight):**

```
5V ────────────────── Module VCC
Module IN ─────────── BC547 Collector
BC547 Emitter ─────── GND
GPIO18 ──[3.3kΩ]────── BC547 Base
BC547 Base ──[21.5kΩ]─ GND
```

BC547 pinout (flat side facing you, legs down): Left=C, Middle=B, Right=E

```
   ┌───────┐
   │ BC547 │   ← flat side toward you
   └┬──┬──┬┘
    C  B  E
```

**Connections — remove the old GPIO18 → IN wire first:**

| # | From | To |
|---|------|-----|
| 1 | BC547 **left leg** (C) | Relay module **IN** |
| 2 | BC547 **right leg** (E) | **GND** |
| 3 | BC547 **middle leg** (B) | **3.3kΩ** → **GPIO18** |
| 4 | BC547 **middle leg** (B) | **21.5kΩ** → **GND** |
| 5 | Relay **VCC** | **5V** |
| 6 | Relay **GND** | **GND** — common with the ESP32 |

Both resistors land on the middle leg: one up to GPIO18, one down to GND.

Think of the transistor as a remote-controlled jumper wire between IN and GND,
with GPIO18 as the switch:

- GPIO18 **LOW** → no base current → C and E isolated → IN left open → relay OFF
- GPIO18 **HIGH** → base current → C connects to E → IN pulled to GND → relay ON

No pull-up on IN is needed. The transistor simply disconnects IN when open,
which is the OFF condition — confirmed by hand with a jumper wire before
building the circuit.

**Why the 3.3kΩ (current limiting).** The base is a diode to the emitter: it
clamps at ~0.7V and then stops resisting, taking whatever current is offered.
Wired direct to a GPIO, that is a short in all but name:

```
(3.3V − 0.7V) / ~40Ω internal ≈ 65mA   vs. pin rated 20mA continuous, 40mA max
```

With the resistor: `(3.3 − 0.7) / 3300 = 0.79mA`. The relay input draws ~4mA and
a BC547 has hFE 200-400, so it needs only ~0.02mA of base current — 0.79mA is
roughly 40× that, driving it hard into saturation. This is **not** a 3.3V-
specific requirement; at 5V the current would be higher and the resistor even
more necessary. (A MOSFET such as the 2N7002 has an insulated, voltage-driven
gate and needs no such resistor — that is a part-type difference, not a
supply-voltage one.)

**Why the 21.5kΩ (idle state).** It carries no working current. GPIO18 is Hi-Z
from reset until `pinMode()` runs, and a floating base is a small antenna —
stray coupling can lift it past 0.7V and partially switch the transistor on.
Harmless flicker on the backlight; a spurious pump start here. The pull-down
drains that to ground. It costs only `0.7V / 21.5kΩ = 0.033mA` of base drive,
about 4%.

Values are forgiving — the **ratio** matters more than the absolutes. Keep the
pull-down roughly 5-10× the base resistor: strong enough to hold the base down
while floating, weak enough not to rob drive when active. 3.3kΩ : 21.5kΩ is
6.5×. Workable alternatives: 4.6kΩ + 46kΩ, or 1kΩ + 10kΩ.

**Staged bench test — do this before connecting GPIO18:**

1. Wire connections 1, 2, 4, 5 and 6. Leave the 3.3kΩ's far end loose.
2. Power the relay module. Loose wire touching nothing → relay **off**.
3. Touch the loose end to **3.3V** → relay clicks **on**.
4. Pull it away → clicks **off**.
5. If that works, plug that wire into GPIO18.

If the relay behaves backwards, the transistor legs are mirrored — rotate it
180° and retry, since the flat face is easy to read the wrong way round.

**Mains side (screw terminals — wire last, mains disconnected):**

| Terminal | Chinese label | Connects to |
|----------|---------------|-------------|
| Left | 常开 (NO) | Live wire TO pump |
| Middle | 公共端 (COM) | Live wire FROM ACS712 IP- |
| Right | 常闭 (NC) | Leave unconnected |

**Logic — settled, do not re-derive.** The transistor inverts, so the module and
the pin disagree. Both of these are true at once:

- At the module: IN LOW = relay ON, IN HIGH = relay OFF (active-LOW)
- At the pin: **GPIO18 HIGH = relay ON**, GPIO18 LOW = relay OFF

```
GPIO18 HIGH → base current → transistor conducts → IN at 0V → relay ON
GPIO18 LOW  → no base current → transistor open   → IN at 5V → relay OFF
```

**config.h:**
```cpp
#define PIN_PUMP_RELAY     18
#define RELAY_ACTIVE_LOW    0   // 0 = HIGH energises — the BC547 inverts
```

---

## 7. I2C Bus (reserved for future expansion)

Not currently connected to any module. Reserved pins:

```cpp
#define PIN_I2C_SDA   8
#define PIN_I2C_SCL   9
```

---

## 8. Radio upgrade bus (reserved — RFM69HCW, SPI3/HSPI)

Not currently connected. Reserved for future RFM69 upgrade.
**Do not use these pins for anything else.**

```cpp
#define PIN_RADIO_SCLK   42
#define PIN_RADIO_MOSI    1
#define PIN_RADIO_MISO    2
#define PIN_RADIO_CS     15
#define PIN_RADIO_IRQ    17
#define PIN_RADIO_RST    16   // currently also used as ASK RX data pin
```

---

## 9. RGB status LED (onboard WS2812, GPIO48)

Onboard LED — driven by GPIO48 via NeoPixel protocol.
Verify jumper state on your board before use.

```cpp
// PIN_RGB_LED   48   (add to config.h when StatusLED module is implemented)
```

---

## Mains wiring sequence (complete last, mains disconnected)

The full mains signal path from wall to pump:

```
Wall socket
├── NEUTRAL ──────────────────────────────────────── Pump NEUTRAL
└── LIVE ──[FUSE]──┬── ZMPT101B AC1 (voltage tap, parallel)
                   │   ZMPT101B AC2 ── Neutral
                   │
                   └── ACS712 IP+ ── ACS712 IP- ── Relay COM ──[NO]── Pump LIVE
```

**Always add a fuse** on the Live line rated for your pump's current (typically 5–10A).
**Never probe mains with board powered.** Mains work only with wall plug removed.

---

## Calibrating the power sensors

Both sensors ship uncalibrated and the firmware cannot guess either constant.
Work through this in order — each step depends on the one before it being right.

**Safety.** The ZMPT101B AC terminals and the ACS712 IP+/IP- pads sit at mains
potential. Do not touch either while the wall plug is in. Do not connect the
ESP32's USB to a non-isolated laptop while mains is live on the sensor board —
use a USB isolator, a phone charger with the serial log read over the display,
or pull the plug before touching the board.

Set `POWER_CAL_LOG 1` in `config.h` and flash. `[PowerMeter]` prints one line a
second:

```
[PowerMeter]  231.4V  4.62A  912.3W 50.0Hz | pin  460.0/ 209.4mV bias 2348/1715mV n=1962
[PowerMeter]   range V 1888..2808mV (head 192mV)  I 1506..1924mV (head 1076mV)
```

### Step 1 — sample rate (`n`)

Check this first; every other number on the line is only as good as the rate
that produced it. `n` should be close to `1000000 / ADC_SAMPLE_INTERVAL_US`,
i.e. **~2000** at the default 500 µs.

If it is materially lower, `analogReadMilliVolts()` is too slow on this build.
Either raise `ADC_SAMPLE_INTERVAL_US` to match what the loop actually sustains,
or switch `PowerMeter.cpp` to plain `analogRead()` with a linear mV conversion.
Anything at or above ~1000 still gives 20 samples per 50 Hz cycle, which is
enough for a stable RMS. Do not proceed until `n` is stable.

### Step 2 — set the trim pot, then measure `ZMPT_CAL_V_PER_V`

**Order matters: pot first, constant second.** The constant describes where the
pot is, so touching the pot afterwards voids it.

With mains live and no load running, watch the `range` line's voltage `head`
figure. Turn the trim pot until:

- no `CLIPPING` warning appears, and
- `head` settles somewhere around **200-400 mV**

Do not chase a larger signal by squeezing `head` toward zero — mains fluctuates,
and a peak that just fits today clips on the next surge. Do not leave it near
the full ~650 mV either; you are throwing away resolution for no benefit.

Then take `pin` (the first of the two figures, mV RMS) off the log line and
read true supply voltage off a multimeter:

```
ZMPT_CAL_V_PER_V = measured_volts / (pin_mV / 1000)
```

A meter reading 231.4 V against a logged `460.0 mV` gives
`231.4 / 0.460 = 503.0`. Set the constant, reflash, and confirm the reported
voltage tracks the meter within a volt or two.

Expect a value in the hundreds on this board — the 5V-supplied module with no
divider produces a small pin signal, so the constant that scales it back up is
correspondingly large. That is normal, not a sign of an error.

### Step 3 — current and power (`ACS712_MV_PER_AMP`)

Run a **resistive** load — a kettle or a heat gun, not the pump. Resistive means
power factor ≈ 1, so watts should equal volts × amps, which is what makes it
usable as a reference. Compare the reported watts against a plug-in energy
meter.

If they disagree by a consistent ratio, scale `ACS712_MV_PER_AMP` by it; the
nominal 66 mV/A is a datasheet typical and real parts vary. If current reads a
drifting non-zero with everything switched off, raise `POWER_NOISE_FLOOR_A`
until it reports a clean 0.

### Step 4 — power factor, against the pump

Now run the pump. Reported watts must land **noticeably below** volts × amps —
an induction motor's power factor is around 0.7–0.85, and that gap is the whole
reason the firmware integrates `mean(v×i)` instead of multiplying the two RMS
figures.

If watts equals volts × amps on the pump, the real-power path is broken. Fix it
before going any further: energy accumulation integrates this number, so the
error would be baked into every kWh total from then on.

### Step 5 — turn the log off

Set `POWER_CAL_LOG 0` and reflash. Record the constants you landed on in the
commit message — they are board-specific and there is no way to recover them
except by repeating this procedure.

---

## GPIO status summary

| GPIO | Status | Connected to |
|------|--------|--------------|
| 1 | Reserved | RFM69 MOSI (future) |
| 2 | Reserved | RFM69 MISO (future) |
| 4 | Active | ACS712 OUT (via divider) |
| 5 | Active | ZMPT101B OUT |
| 6 | Active | Button DOWN |
| 7 | Claimed | RH_ASK unused TX — driven by the library, leave unwired |
| 8 | Reserved | I2C SDA |
| 9 | Reserved | I2C SCL |
| 10 | Claimed | RH_ASK unused PTT — driven by the library, leave unwired |
| 11 | Active | Button LEFT |
| 12 | Active | Button UP |
| 13 | Active | Button SELECT / OK |
| 14 | Active | Button RIGHT |
| 15 | Reserved | RFM69 CS (future) |
| 16 | Active | ASK RX data (→ RFM69 RST after upgrade) |
| 17 | Reserved | RFM69 IRQ (future) |
| 18 | Active | Pump relay IN |
| 19 | System | Native USB D- |
| 20 | System | Native USB D+ |
| 21 | Active | Backlight BC547 base (via 2kΩ) |
| 38 | Active | TFT DC |
| 39 | Active | TFT CS |
| 40 | Active | TFT SCLK (SPI2) |
| 41 | Active | TFT MOSI (SPI2) |
| 42 | Reserved | RFM69 SCLK (future) |
| 43 | System | UART0 TX |
| 44 | System | UART0 RX |
| 47 | Active | TFT RST |
| 48 | Active | Onboard RGB LED |