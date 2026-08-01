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
#define PIN_SOLENOID       10

// ---------- I2C (reserved for expansion) ----------
#define PIN_I2C_SDA         8
#define PIN_I2C_SCL         9

// ---------- Spare / expansion ----------
// Analog spares (ADC1): GPIO7
// 3rd SPI device: joins HSPI radio bus + 1 CS pin (GPIO6/7, or GPIO48 if free)
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
