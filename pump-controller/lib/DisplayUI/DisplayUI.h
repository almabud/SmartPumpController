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

    // Boot screen — called from setup() only, before the scheduler starts.
    void showBoot(uint8_t step, uint8_t total, const char* label);

private:
    ScreenId _currentScreen = ScreenId::HOME;
    bool     _screenChanged = true;
    uint8_t  _menuIndex     = 0;

    void _handleNavigation(ButtonEvent event);
    void _goTo(ScreenId screen);
    const char* _getScreenTitle(ScreenId screen);

    // Boot screen drawing
    void _drawBoot(uint8_t step, uint8_t total, const char* label);
    // Home screen drawing
    void _drawHome(SystemState& state);
    // Title bar drawing
    void _drawTitleBar(SystemState& state);
    void _drawSignalBars(uint8_t level);
    uint8_t _rssiToBars(int8_t rssi, bool connected);
    void _drawHeartbeat(SystemState& state);
    void _drawCloudIcon(bool connected);
    // Tank level drawing
    void _drawTankLevel(SystemState& state);
    // Tank temperature drawing
    void _drawTankTemp(SystemState& state);
    // Pump state drawing
    void _drawPumpState(SystemState& state);
    // Timer drawing
    void _drawPumpTimer(SystemState& state);
};
