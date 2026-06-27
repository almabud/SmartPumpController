# Wiring Reference — water-tank (Arduino Nano ATmega328P)

> Permanent wiring reference for the Nano sensor node.
> All pin numbers match `water-tank/include/config.h` exactly.
> **Never modify wiring without updating config.h and this document together.**
>
> Board: Arduino Nano (ATmega328P, 5V native logic)
> Note: Nano runs at 5V — no voltage dividers needed on this board.
> All sensors are powered from Nano's 5V pin.

---

## Power rails

| Source | Voltage | Notes |
|--------|---------|-------|
| USB or VIN | 5V input | Powers the Nano |
| Nano 5V pin | 5V output | Powers all three modules |
| Common GND | 0V | All modules share this rail |

---

## 1. AJSR04M Waterproof Ultrasonic Sensor

**Purpose:** measures distance from sensor to water surface for tank level calculation.
**Mount:** at the top of the tank lid, pointing straight down toward the water.

| Module Pin | Connects to | Notes |
|------------|-------------|-------|
| VCC (+5V) | 5V | |
| GND | GND | |
| TRIG | D2 | Send trigger pulse |
| ECHO | D3 | Receive echo pulse |

**Range:** 20cm–450cm, ±1cm accuracy.
**Blind zone:** do not mount closer than 20cm to the water surface at maximum fill.

**Physical installation note:** mount DS18B20 temperature sensor physically adjacent
to the AJSR04M — both measure conditions at the same air space above the water,
and the DS18B20 reading is used to temperature-correct the speed-of-sound calculation
for accurate distance readings.

**Condensation note:** mount with sensor face pointing straight down so water
droplets do not pool on the face. Consider a small drip shield above the module PCB.

**config.h pins:**
```cpp
#define PIN_SENSOR_TRIG   2
#define PIN_SENSOR_ECHO   3
```

---

## 2. 433 MHz ASK Transmitter (7-pin module)

**Purpose:** transmits SensorPacket (tank level + temperature) to ESP32 receiver.
**One-way link:** this board transmits only, ESP32 receives only.

| Module Pin | Connects to | Notes |
|------------|-------------|-------|
| ANT (pin 1) | 17.3cm straight wire | Primary antenna — mandatory |
| GND (pin 2) | GND | |
| VCC (pin 3) | 5V | |
| CS  (pin 4) | 5V | Tied HIGH permanently — always enabled |
| DATA (pin 5) | D4 | Data signal from Nano |
| GND (pin 6) | GND | Same rail as pin 2 |
| ANT (pin 7) | 17.3cm wire or floating | Second antenna connection |

**CS pin:** tied permanently to 5V — no GPIO needed. Module stays always active.
Nano firmware uses `delay()` between transmissions for timing, not CS sleep mode.

**Antenna:** solder a 17.3cm straight, solid-core wire to ANT (pin 1).
Keep it vertical and uncoiled — this is the single most important factor for range.
Pin 7 (second ANT): optionally add a second identical wire, or leave floating.

**config.h pin:**
```cpp
#define PIN_RF433_TX   4
// CS tied to 5V — no pin defined, no GPIO needed
```

---

## 3. DS18B20 Waterproof Temperature Sensor

**Purpose:** measures air temperature near the ultrasonic sensor for speed-of-sound
correction. NOT measuring water temperature — measures the air gap above the water.
Mount physically next to the AJSR04M on the tank lid.

**Wire colour coding (standard across most manufacturers):**

| Wire colour | Signal | Connects to |
|-------------|--------|-------------|
| Red | VCC | 5V |
| Black | GND | GND |
| Yellow (or Blue) | DATA (DQ) | D5 + pull-up |

**Wiring with pull-up resistor:**

```
5V ──[4.6kΩ]──┬──── D5
               │
           DS18B20 DATA (yellow/blue wire)
```

**The 4.6kΩ pull-up is mandatory** — the Nano's internal pull-up (20–50kΩ)
is too weak for reliable 1-Wire communication. External pull-up required.
Your 4.6kΩ is close enough to the standard 4.7kΩ — confirmed usable.

**Important:** uses 1-Wire (OneWire) protocol — a single data pin carries
all communication. Only one external resistor needed regardless of how many
DS18B20 sensors are on the same bus (you have one).

**config.h pin:**
```cpp
#define PIN_TEMP_DATA   5
```

---

## 4. Tank physical constants (tune to your actual tank)

These values must be measured and set correctly in config.h before the
level percentage calculation will be accurate:

```cpp
#define TANK_HEIGHT_CM        100   // distance from sensor face to tank bottom (empty)
#define TANK_SENSOR_OFFSET_CM   5   // distance from sensor face to water at 100% full
```

**How to measure:**
- `TANK_HEIGHT_CM`: with tank empty, measure from sensor face down to tank floor
- `TANK_SENSOR_OFFSET_CM`: the minimum distance the sensor needs to the water
  surface (AJSR04M blind zone is 20cm — this must be at least 20)

**Level calculation in firmware:**
```cpp
float distanceCm = measureDistance(temperatureC);   // temperature-corrected
float levelPct = 100.0f - ((distanceCm - TANK_SENSOR_OFFSET_CM) /
                 (TANK_HEIGHT_CM - TANK_SENSOR_OFFSET_CM) * 100.0f);
levelPct = constrain(levelPct, 0.0f, 100.0f);
```

---

## 5. Reserved / unused pins

| Pin | Status | Notes |
|-----|--------|-------|
| D0 (RX) | System | UART — do not use (conflicts with USB upload) |
| D1 (TX) | System | UART — do not use (conflicts with USB upload) |
| D10–D13 | Available | Hardware SPI — free for future use |
| A4 | Available | I2C SDA — free for future use |
| A5 | Available | I2C SCL — free for future use |
| A0–A3 | Available | Analog inputs — free for future sensors |

---

## GPIO status summary

| Pin | Status | Connected to |
|-----|--------|--------------|
| D0 | System | UART RX (do not use) |
| D1 | System | UART TX (do not use) |
| D2 | Active | AJSR04M TRIG |
| D3 | Active | AJSR04M ECHO |
| D4 | Active | 433 TX DATA |
| D5 | Active | DS18B20 DATA (+ 4.6kΩ pull-up to 5V) |
| D6–D9 | Free | Available |
| D10–D13 | Free | Hardware SPI (available) |
| A0–A5 | Free | Analog / digital (available) |

---

## Complete physical wiring summary

```
Arduino Nano
                    ┌─────────────┐
              D2 ───┤ TRIG        │ AJSR04M
              D3 ───┤ ECHO        │ ultrasonic
                    └─────────────┘

                    ┌─────────────┐
              D4 ───┤ DATA (pin5) │
         5V ────────┤ VCC (pin3)  │ 433 TX module
         5V ────────┤ CS  (pin4)  │ (always enabled)
        GND ────────┤ GND (pin2,6)│
              ANT ──┤ ANT (pin1)  │ 17.3cm wire
                    └─────────────┘

              D5 ───┬─── DATA (yellow) ─── DS18B20
[4.6kΩ: 5V to D5] ─┘
         5V ─────────── VCC (red)  ──── DS18B20
        GND ─────────── GND (black)──── DS18B20

         5V ─── all module VCC pins
        GND ─── all module GND pins
```

---

## Libraries required

Declared in `water-tank/platformio.ini`:

```ini
lib_deps =
    radiohead/RadioHead @ ^1.122
    paulstoffregen/OneWire @ ^2.3.7
    milesburton/DallasTemperature @ ^3.9.0
    teckel12/NewPing @ ^1.9.7
```

| Library | Used for |
|---------|---------|
| RadioHead (RH_ASK) | 433 ASK transmission |
| OneWire | DS18B20 1-Wire protocol |
| DallasTemperature | DS18B20 temperature reading |
| NewPing | AJSR04M distance measurement |