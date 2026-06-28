#include "DisplayUI.h"

TFT_eSPI    _tft;
TFT_eSprite _sprite(&_tft);

void DisplayUI::begin() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    _tft.init();
    _tft.setRotation(1);
    _tft.fillScreen(TFT_BLACK);

    _sprite.setColorDepth(16);
    _sprite.createSprite(160, 128);
    _sprite.setSwapBytes(true);

    Serial.println("[DisplayUI] begin - TFT_eSPI ready");
}

void DisplayUI::update(SystemState& state, ButtonEvent event) {
   _handleNavigation(event);

   if (!state.hasChanged(Consumer::DISPLAY_CONSUMER) && !_screenChanged)
        return;

    switch (_currentScreen) {
        case ScreenId::HOME: _drawHome(state); break;
    }

    _sprite.pushSprite(0, 0);
    state.markSeen(Consumer::DISPLAY_CONSUMER);
    _screenChanged = false;
}

void DisplayUI::_handleNavigation(ButtonEvent event) {
    // navigation added as screens are implemented
}

void DisplayUI::_goTo(ScreenId screen) {
    _currentScreen = screen;
    _screenChanged = true;
}
