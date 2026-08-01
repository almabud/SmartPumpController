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
```

---

## 5. Voltage Sensor (ZMPT101B)

**Note:** Power from 3.3V so output naturally stays within ADC range. No voltage divider needed.

| Module Pin | Connects to | Notes |
|------------|-------------|-------|
| VCC | 3.3V | NOT 5V — keeps output within ADC range |
| GND | GND | |
| OUT | GPIO5 | Direct connection, no divider |
| AC Terminal 1 | Live (mains) | Parallel connection — wire last |
| AC Terminal 2 | Neutral (mains) | Parallel connection — wire last |

**Important:** tune the onboard potentiometer so the AC waveform peak stays just under 3.3V on GPIO5 before trusting any voltage readings in firmware.

**config.h pin:**
```cpp
#define PIN_VOLTAGE_SENSE   5
```

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

## GPIO status summary

| GPIO | Status | Connected to |
|------|--------|--------------|
| 1 | Reserved | RFM69 MOSI (future) |
| 2 | Reserved | RFM69 MISO (future) |
| 4 | Active | ACS712 OUT (via divider) |
| 5 | Active | ZMPT101B OUT |
| 6 | Active | Button DOWN |
| 7 | Spare | ADC1 spare |
| 8 | Reserved | I2C SDA |
| 9 | Reserved | I2C SCL |
| 10 | Spare | Free |
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