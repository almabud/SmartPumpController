// lib/DisplayUI/DisplayUI.cpp
#include "DisplayUI.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

// TFT driver and full-screen sprite
// Sprite lives in PSRAM automatically on N16R8 — zero internal RAM cost
TFT_eSPI    _tft;
TFT_eSprite _sprite(&_tft);

void DisplayUI::begin() {
    // Backlight on via BC547 transistor
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);

    // Init display
    _tft.init();
    _tft.setRotation(1);           // landscape — same as v1
    _tft.fillScreen(TFT_BLACK);

    // Create full-screen sprite in PSRAM
    // 160x128 x 2 bytes = 40KB — goes to PSRAM automatically
    _sprite.setColorDepth(16);
    _sprite.createSprite(160, 128);
    _sprite.setSwapBytes(true);

    // Boot splash — draw into sprite, push once
    _sprite.fillSprite(TFT_BLACK);
    _sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    _sprite.setTextSize(1);
    _sprite.setCursor(5, 50);
    _sprite.print("Pump Controller v2");
    _sprite.setCursor(5, 65);
    _sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    _sprite.print("Booting...");
    _sprite.pushSprite(0, 0);

    Serial.println("[DisplayUI] begin - TFT_eSPI + sprite ready");
}

void DisplayUI::update(SystemState& state, ButtonEvent event) {
    _handleNavigation(event);

    if (!state.hasChanged(Consumer::DISPLAY_CONSUMER) && !_screenChanged)
        return;

    // Draw current screen into sprite
    switch (_currentScreen) {
        case ScreenId::HOME:    _drawHome(state);    break;
        case ScreenId::MENU:    _drawMenu(state);    break;
        default:                _drawHome(state);    break;
    }

    // Push entire frame to display — one SPI burst, zero flicker
    _sprite.pushSprite(0, 0);

    state.markSeen(Consumer::DISPLAY_CONSUMER);
    _screenChanged = false;
}

void DisplayUI::_handleNavigation(ButtonEvent event) {
    if (event == ButtonEvent::NONE) return;

    switch (_currentScreen) {
        case ScreenId::HOME:
            if (event == ButtonEvent::SELECT_PRESS)
                _goTo(ScreenId::MENU);
            break;

        case ScreenId::MENU:
            if (event == ButtonEvent::UP_PRESS) {
                if (_menuIndex > 0) _menuIndex--;
            }
            if (event == ButtonEvent::DOWN_PRESS) {
                if (_menuIndex < 3) _menuIndex++;
            }
            if (event == ButtonEvent::BACK_PRESS ||
                event == ButtonEvent::BACK_LONGPRESS)
                _goTo(ScreenId::HOME);
            break;

        default:
            if (event == ButtonEvent::BACK_PRESS ||
                event == ButtonEvent::BACK_LONGPRESS)
                _goTo(ScreenId::HOME);
            break;
    }
}

void DisplayUI::_goTo(ScreenId screen) {
    _currentScreen = screen;
    _screenChanged = true;
}