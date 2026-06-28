#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../SystemState/SystemState.h"
#include "../InputManager/InputManager.h"


enum class ScreenId : uint8_t {
    HOME
};


class DisplayUI {
public:
    void begin();
    void update(SystemState& state, ButtonEvent event);

private:
    ScreenId _currentScreen = ScreenId::HOME;
    bool     _screenChanged = true;
    uint8_t  _menuIndex     = 0;

    void _handleNavigation(ButtonEvent event);
    void _goTo(ScreenId screen);

    // Home screen drawing
    void _drawHome(SystemState& state);
    void _drawTitleBar(SystemState& state);
    void _drawSignalBars(uint8_t level);
    uint8_t _rssiToBars(int8_t rssi, bool connected);
};
