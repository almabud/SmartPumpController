// pump-controller/include/config.h
#pragma once

// ---------- Analog sensors (ADC1 only — Wi-Fi always on) ----------
// The ACS712 is the 5V part: its output reaches ~4.5V, so it is the one behind
// the divider. The ZMPT runs off 3V3 and stays in range on its own.
#define PIN_CURRENT_SENSE   4    // ACS712-30A (via 2.1k/4.6k divider!)
#define PIN_VOLTAGE_SENSE   5    // ZMPT101B (3.3V supply, pot-tuned, no divider)

// ---------- Buttons (touch-capable; active-low push buttons) ----------
#define PIN_BTN_LEFT        11
#define PIN_BTN_UP          12
#define PIN_BTN_SELECT      13    // OK / confirm
#define PIN_BTN_RIGHT       14
#define PIN_BTN_DOWN         6

// ---------- Display bus: SPI2 / FSPI (dedicated, active) ----------
#define PIN_TFT_SCLK       40
#define PIN_TFT_MOSI       41
#define PIN_TFT_CS         39
#define PIN_TFT_DC         38
#define PIN_TFT_RST        47
#define PIN_TFT_BL         21    // optional PWM; tie to 3V3 to skip

// ---------- Radio: ACTIVE NOW = 433 ASK (single data pin, RH_ASK) ----------
#define PIN_RF433_RX       16
#define RF433_BITRATE    2000    // must match water_tank/include/config.h

// RH_ASK's constructor claims a TX and a PTT pin whether or not this node ever
// transmits. Hand it real free pins and leave them unwired — the library's own
// ESP32 example uses GPIO0, which is a strapping pin on the S3.
#define PIN_RF433_TX_UNUSED    7
#define PIN_RF433_PTT_UNUSED  10

// ---------- Radio: RESERVED future RFM69HCW on SPI3 / HSPI ----------
#define PIN_RADIO_SCLK     42
#define PIN_RADIO_MOSI      1    // reserved (idle under ASK)
#define PIN_RADIO_MISO      2    // reserved (idle under ASK)
#define PIN_RADIO_CS       15
#define PIN_RADIO_IRQ      17    // RFM69 DIO0
#define PIN_RADIO_RST      16    // reuses the ASK data pin after upgrade

// ---------- Actuators ----------
#define PIN_PUMP_RELAY     18

// ---------- I2C (reserved for expansion) ----------
#define PIN_I2C_SDA         8
#define PIN_I2C_SCL         9

// ---------- Spare / expansion ----------
// GPIO7 and GPIO10 are now claimed by RH_ASK as its unused TX/PTT pins. They
// carry no signal and nothing is wired to them, but the library drives them as
// outputs, so they are no longer available.
// 3rd SPI device: joins HSPI radio bus + 1 CS pin (GPIO48 if free)
// GPIO48: verify onboard RGB; free it if present

// ---------- Calibration (tune on real hardware) ----------
#define ACS712_DIVIDER_RATIO   0.686f   // 4.6 / (2.1 + 4.6) — your actual resistors
#define ACS712_MV_PER_AMP      66.0f

// ---------- Power metering ----------
// One sample pair per eligible loop pass rather than a blocking burst, so the
// display never waits on the sampler. 2 kHz gives 40 samples per 50 Hz cycle.
#define ADC_SAMPLE_INTERVAL_US   500
#define POWER_WINDOW_MS         1000   // one reported reading per window

// The ZMPT101B has a trim pot, so no constant is right until it is measured on
// this board — see "Calibrating the power sensors" in docs/wiringe_guide.md.
// Mains volts per volt seen at PIN_VOLTAGE_SENSE.
#define ZMPT_CAL_V_PER_V       220.0f  // PLACEHOLDER — calibrate before trusting a reading

// Below the noise floor the ACS712 is only reporting itself; report a clean 0
// instead of a drifting tenth of an amp with the pump off.
#define POWER_NOISE_FLOOR_A      0.15f
#define POWER_V_MIN            180.0f  // sustained under this -> powerFault
#define POWER_V_MAX            260.0f  // sustained over this  -> powerFault
#define POWER_FAULT_CONFIRM_N       3  // consecutive bad windows before the flag latches

// Prints the raw per-window figures the calibration procedure needs, once a
// second. Leave it on until ZMPT_CAL_V_PER_V has been measured and the watts
// have been checked against a plug meter, then set it to 0 — it is noisy enough
// to bury the other modules' log lines.
#define POWER_CAL_LOG               1

// The usable ADC window. Outside it the converter compresses rather than
// tracking, so a clipped peak makes the RMS read LOW while looking perfectly
// healthy — the one failure mode that silently corrupts a calibration.
//
// This matters most on the voltage channel: the ZMPT101B here runs on 5V with
// no divider, so its output biases near 2350mV and only has ~650mV of room
// above before it hits the ceiling. The trim pot is what keeps it inside.
#define ADC_CLIP_HIGH_MV         3000
#define ADC_CLIP_LOW_MV           100
#define POWER_CLIP_WARN_MS       5000   // rate limit for the clipping warning

// ---------- 24h stats ----------
// Bucket period. Drop to 60000 to make an "hour" one minute and roll the whole
// 24-slot ring in 24 minutes while testing.
#define POWER_STATS_BUCKET_MS  3600000UL
#define POWER_STATS_BUCKETS          24
#define POWER_STATS_FLUSH_MS    600000UL  // flush the live bucket to NVS this often, if dirty
#define POWER_STATS_NVS_VER           1   // bump to discard buckets after a struct change

// How often runtime, energy and current are folded into the live bucket. Also
// the resolution of the runtime figure, so there is no reason to go finer.
#define POWER_STATS_TICK_MS        1000

// Totals log cadence. Set to 0 to silence once the numbers are trusted — the
// home screen is where these belong, this is for bringing the module up.
#define POWER_STATS_LOG_MS        10000

// ---------- Tank calibration (placeholders — measure on site) ----------
// The distances the sensor reports at the two extremes. Read them off the
// [RadioReceiver] log line with the tank full and then empty. These are only
// the power-on defaults; they become user-editable from the config page.
#define TANK_DISTANCE_FULL_MM    300   // TODO reading when the tank is FULL
#define TANK_DISTANCE_EMPTY_MM  1500   // TODO reading when the tank is EMPTY

#define TANK_STALE_TIMEOUT_MS  10000   // 5 missed packets at the node's 2s send interval

// Link diagnostics cadence. Set to 0 to silence once the link is trusted.
#define RADIO_STATS_INTERVAL_MS 5000

// ---------- Scheduler intervals ----------
#define INTERVAL_RADIO_MS        50
#define INTERVAL_DISPLAY_MS     500
#define INTERVAL_WIFI_CHECK_MS 5000
#define INTERVAL_CLOUD_MS      2000

// ---------- Input timing ----------
#define BUTTON_DEBOUNCE_MS       30
#define BUTTON_LONG_PRESS_MS   1000   // hold SELECT this long to toggle the pump

// A widget left focused or an edit left half-finished backs out on its own, so
// the display never sits waiting on a user who walked away. Editing gets the
// longer window — setting a timer has natural pauses.
#define UI_FOCUS_TIMEOUT_MS  10000   // no press for this long drops the focus
#define UI_EDIT_TIMEOUT_MS   30000   // no press for this long discards the edit

// ---------- Pump safety ----------
// The relay module is 5V-logic: a 3.3V IN reads as LOW, so the pin alone can
// never release it. GPIO18 drives a BC547 low-side switch instead, which
// inverts — so HIGH at the pin energises the relay, even though the module
// itself is active-LOW at its IN terminal. See docs/wiringe_guide.md section 6.
#define RELAY_ACTIVE_LOW          0   // 0 = HIGH energises the relay
#define PUMP_MIN_OFF_MS       3000   // minimum OFF time before a restart is allowed

// ---------- Pump timer ----------
#define TIMER_MINUTE_STEP         1   // minutes per UP/DOWN press (0..59)
#define TIMER_MAX_HOURS          23   // hour field wraps 0..23

// ---------- Boot screen ----------
#define FIRMWARE_VERSION     "v2.0"
#define BOOT_TOTAL_STEPS         6   // Display, Inputs, Radio, Power, Stats, Ready — bump as modules land
#define BOOT_STEP_MIN_MS       500   // per-step dwell so the progress bar visibly fills
#define BOOT_HOLD_MS           500   // extra hold on the completed bar before the home screen
