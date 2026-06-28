// lib/DisplayUI/DisplayUI.h
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../SystemState/SystemState.h"
#include "../InputManager/InputManager.h"

enum class ScreenId : uint8_t {
    HOME,
    MENU,
    MANUAL_CONTROL,
    SCENES,
    SETTINGS,
    ABOUT
};

class DisplayUI {
public:
    void begin();
    void update(SystemState& state, ButtonEvent event);

private:
    // Navigation state
    ScreenId _currentScreen = ScreenId::HOME;
    bool     _screenChanged = true;
    uint8_t  _menuIndex     = 0;

    // Navigation methods
    void _handleNavigation(ButtonEvent event);
    void _goTo(ScreenId screen);

    // Screen draw methods (implemented in separate files)
    void _drawHome(SystemState& state);
    void _drawMenu(SystemState& state);
    void _drawManualControl(SystemState& state);
    void _drawScenes(SystemState& state);
    void _drawSettings(SystemState& state);
    void _drawAbout(SystemState& state);
};