// pump-controller/include/config.h
#pragma once

// ---------- Analog sensors (ADC1 only — Wi-Fi always on) ----------
#define PIN_VOLTAGE_SENSE   4    // ZMPT101B (3.3V supply, pot-tuned)
#define PIN_CURRENT_SENSE   5    // ACS712-30A (via 10k/20k divider!)

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
// Analog spares (ADC1): GPIO7
// General-purpose spare: GPIO10
// 3rd SPI device: joins HSPI radio bus + 1 CS pin (GPIO7/10, or GPIO48 if free)
// GPIO48: verify onboard RGB; free it if present

// ---------- Calibration (tune on real hardware) ----------
#define ACS712_DIVIDER_RATIO   0.686f   // 4.6 / (2.1 + 4.6) — your actual resistors
#define ACS712_MV_PER_AMP      66.0f

// ---------- Scheduler intervals ----------
#define INTERVAL_RADIO_MS        50
#define INTERVAL_POWER_MS      1000
#define INTERVAL_DISPLAY_MS     500
#define INTERVAL_WIFI_CHECK_MS 5000
#define INTERVAL_CLOUD_MS      2000

// ---------- Input timing ----------
#define BUTTON_DEBOUNCE_MS       30
#define BUTTON_LONG_PRESS_MS   1000   // hold SELECT this long to toggle the pump

// ---------- Pump safety ----------
// The relay module is 5V-logic: a 3.3V IN reads as LOW, so the pin alone can
// never release it. GPIO18 drives a BC547 low-side switch instead, which
// inverts — so HIGH at the pin energises the relay, even though the module
// itself is active-LOW at its IN terminal. See docs/wiringe_guide.md section 6.
#define RELAY_ACTIVE_LOW          0   // 0 = HIGH energises the relay
#define PUMP_MIN_OFF_MS       30000   // minimum OFF time before a restart is allowed

// ---------- Pump timer ----------
#define TIMER_MINUTE_STEP         1   // minutes per UP/DOWN press (0..59)
#define TIMER_MAX_HOURS          23   // hour field wraps 0..23

// ---------- Boot screen ----------
#define FIRMWARE_VERSION     "v2.0"
#define BOOT_TOTAL_STEPS         3   // Display, Inputs, Ready — bump as modules land
#define BOOT_STEP_MIN_MS       500   // per-step dwell so the progress bar visibly fills
#define BOOT_HOLD_MS           500   // extra hold on the completed bar before the home screen
