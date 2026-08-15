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

// 100 mV/A is the 20A variant. Measured: a ~750W element on a 202V tap should
// draw 2.87A, and the 30A figure of 66 mV/A read it as 4.5A — 57% high. The
// v1 firmware assumed 100 mV/A as well. Confirm against the marking on the
// chip itself (ACS712ELCTR-05B / -20A / -30A) before trusting this.
#define ACS712_MV_PER_AMP     100.0f

// The ACS712 measures *directed* current, so which way round it sits in the
// load loop sets the sign of real power. Wired one way a consuming load reads
// as generating. -1 corrects it here rather than re-doing mains wiring, which
// is the more dangerous of the two fixes.
#define ACS712_DIRECTION         -1

// ---------- Power metering ----------
// One sample pair per eligible loop pass rather than a blocking burst, so the
// display never waits on the sampler. 2 kHz gives 40 samples per 50 Hz cycle.
#define ADC_SAMPLE_INTERVAL_US   500
#define POWER_WINDOW_MS         1000   // one reported reading per window
#define MAINS_NOMINAL_HZ        50.0f  // only used to convert the phase constant to samples

// The ZMPT101B delays the voltage waveform relative to reality; the ACS712's
// Hall sensor does not. Left uncorrected the two channels are misaligned and
// mean(v*i) reads low by cos(error) on every measurement — a systematic loss
// that every watt and every kWh inherits.
//
// Measured on this board against a purely resistive 68.9 ohm load, where true
// power factor is 1 by definition: 204.7V x 2.97A = 607.9VA apparent against
// 544.8W real, so cos = 0.896 and the error is 26.3 degrees.
//
// Positive delays the CURRENT channel, negative delays VOLTAGE. Negative here:
// measured the other way first and pf fell from 0.896 to 0.606, i.e. the error
// grew by exactly the 26.3 applied, so it is the current channel that arrives
// late. Most likely the 2.1k/4.6k divider on the ACS712 output forming an RC
// lag with whatever capacitance sits on that node — the ZMPT feeds its pin
// directly, with no divider.
//
// Retune against a resistive load: correct when reported W equals V x A, i.e.
// the pf field in the calibration log reads 1.000.
#define ZMPT_PHASE_CAL_DEG     -26.3f

// The ZMPT101B has a trim pot, so no constant is right until it is measured on
// this board — see "Calibrating the power sensors" in docs/wiringe_guide.md.
// Mains volts per volt seen at PIN_VOLTAGE_SENSE.
// Measured on this board: 230.0V at the wall against 366.7mV RMS at the pin.
// Large because the module runs on 5V with no divider, so the pin signal is
// small. Void if the trim pot is touched.
//
// Taken with NO load running, which matters more than it looks. Under load the
// wall socket and the sensor's tap point stop being the same node — current
// through the wiring between them drops real volts, and metering one while
// sampling the other bakes that drop into the constant. Calibrated under 750W
// this came out as 721 and read 14% high once the load was removed.
#define ZMPT_CAL_V_PER_V       627.0f

// Below the noise floor the ACS712 is only reporting itself; report a clean 0
// instead of a drifting tenth of an amp with the pump off.
#define POWER_NOISE_FLOOR_A      0.15f

// Same idea on the voltage channel. ZMPT_CAL_V_PER_V is in the hundreds, so a
// couple of millivolts of ADC noise on a dead sensor scales up into a volt or
// two of phantom mains. Anything real is a mains reading; there is nothing
// legitimate between zero and this, so report a clean 0 below it.
#define POWER_NOISE_FLOOR_V     30.0f
#define POWER_V_MIN            180.0f  // sustained under this -> powerFault
#define POWER_V_MAX            260.0f  // sustained over this  -> powerFault
#define POWER_FAULT_CONFIRM_N       3  // consecutive bad windows before the flag latches

// Set to 1 to print the raw per-window figures once a second: pin millivolts,
// tracked bias, sample count, and the headroom line the trim pot is set by.
// Turn it back on to run the calibration procedure in docs/wiringe_guide.md —
// ZMPT_CAL_V_PER_V cannot be measured without it. Off otherwise; it is noisy
// enough to bury the other modules' log lines.
//
// The CLIPPING warning is deliberately NOT behind this flag. A clipped waveform
// reads low while looking healthy, so it has to be reported whether or not
// anyone is watching for it.
#define POWER_CAL_LOG               0

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
// the power-on defaults — ConfigStore loads whatever the config page last saved
// over them, so measuring on site no longer needs a reflash.
#define TANK_DISTANCE_FULL_MM    300   // TODO reading when the tank is FULL
#define TANK_DISTANCE_EMPTY_MM  1500   // TODO reading when the tank is EMPTY

// What the config page will accept, and what a restored NVS value is checked
// against. Both come from the sensor rather than from the tank: the AJSR04M has
// a blind zone below 20cm and is rated to 4.5m — see water_tank/include/config.h.
#define TANK_MIN_MM              200
#define TANK_MAX_MM             4500

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

// LEFT is "back one level" on a short press, so the hold that jumps all the way
// home has to be clearly longer than a press that overshot — hence 2s, not 1s.
#define BUTTON_BACK_HOLD_MS    2000   // hold LEFT this long to return to the home screen

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

// ---------- Config store ----------
#define CFG_NVS_VER               1   // bump to discard saved settings after a schema change

// ---------- Boot screen ----------
#define FIRMWARE_VERSION     "v2.0"
#define BOOT_TOTAL_STEPS         7   // Display, Inputs, Config, Radio, Power, Stats, Ready — bump as modules land
#define BOOT_STEP_MIN_MS       500   // per-step dwell so the progress bar visibly fills
#define BOOT_HOLD_MS           500   // extra hold on the completed bar before the home screen
