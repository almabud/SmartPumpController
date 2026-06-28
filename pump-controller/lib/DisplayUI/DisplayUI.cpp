#include "DisplayUI.h"

static TFT_eSPI    _tft;
static TFT_eSprite _sprite(&_tft);

void DisplayUI::begin() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    _tft.init();
    _tft.setRotation(1);
    _tft.fillScreen(TFT_BLACK);

    _sprite.setColorDepth(16);
    _sprite.createSprite(160, 128);
    _sprite.setSwapBytes(true);

    Serial.println("[DisplayUI] ready");
}

void DisplayUI::update(SystemState& state, ButtonEvent event) {
    _sprite.fillSprite(TFT_NAVY);

    _sprite.setTextColor(TFT_WHITE, TFT_NAVY);
    _sprite.setTextSize(2);
    _sprite.setCursor(4, 8);
    _sprite.print("Pump Ctrl");

    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_GREEN, TFT_NAVY);
    _sprite.setCursor(4, 36);
    _sprite.printf("Tank : %3d%%", state.tankLevelPct);

    _sprite.setTextColor(state.pumpState == PumpState::ON ? TFT_GREEN : TFT_RED, TFT_NAVY);
    _sprite.setCursor(4, 50);
    _sprite.printf("Pump : %s", state.pumpState == PumpState::ON ? "ON " : "OFF");

    _sprite.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
    _sprite.setCursor(4, 70);
    _sprite.printf("WiFi : %s", state.wifiConnected ? "OK" : "--");
    _sprite.setCursor(4, 82);
    _sprite.printf("Cloud: %s", state.cloudConnected ? "OK" : "--");

    _sprite.pushSprite(0, 0);
}
