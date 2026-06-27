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