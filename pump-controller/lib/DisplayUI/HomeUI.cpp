#include "DisplayUI.h"
#include <TFT_eSPI.h>
#include "FontAwesomesolid9006.h"

extern TFT_eSprite _sprite;

// --- Title bar drawing ---
uint8_t DisplayUI::_rssiToBars(int8_t rssi, bool connected) {
    if (!connected) return 0;   // no signal
    if (rssi >= -50) return 4;  // excellent
    if (rssi >= -65) return 3;  // good
    if (rssi >= -75) return 2;  // fair
    if (rssi >= -85) return 1;  // weak
    return 0;
}

void DisplayUI::_drawSignalBars(uint8_t level){
    const int16_t  X          = 147;
    const int16_t  Y          = 1;
    const uint8_t  BAR_WIDTH  = 2;
    const uint8_t  BAR_GAP    = 1;
    const uint8_t  MAX_HEIGHT = 10;
    const uint16_t COLOR_ON   = TFT_GREEN;
    const uint16_t COLOR_OFF  = TFT_DARKGREY;

    for (uint8_t i = 0; i < 4; i++) {
        uint8_t barHeight = 3 + (i * 2);         // 3, 5, 7, 9
        int16_t bx        = X + i * (BAR_WIDTH + BAR_GAP);
        int16_t by        = Y + (MAX_HEIGHT - barHeight);
        uint16_t color    = (i < level) ? COLOR_ON : COLOR_OFF;
        _sprite.fillRect(bx, by, BAR_WIDTH, barHeight, color);
    }
}

void DisplayUI::_drawHeartbeat(SystemState& state) {
    const int16_t  X        = 139;   // just left of signal bars
    const int16_t  Y        = 7;     // vertically centered in 14px title bar
    const uint8_t  RADIUS   = 3;
    uint16_t color          = !state.tankStale ? TFT_GREEN : TFT_RED;
    // Heartbeat blinks every second
    bool filled             = (state.uptimeSeconds % 2 == 0);

    if (filled) {
        _sprite.fillCircle(X, Y, RADIUS, color);
    } else {
        _sprite.fillCircle(X, Y, RADIUS, TFT_BLACK);  // erase
    }
}

void DisplayUI::_drawCloudIcon(bool connected) {
    const int16_t  x     = 118;
    const int16_t  y     = 1;
    uint16_t color  = connected ? TFT_GREEN : TFT_DARKGREY;
    _sprite.loadFont(FontAwesomesolid9006);
    _sprite.setTextColor(color, TFT_BLACK);
    _sprite.drawString("\uf0c2", x, y);
    _sprite.unloadFont();
}

void DisplayUI::_drawTitleBar(SystemState& state) {
    _sprite.fillRect(0, 0, 160, 14, TFT_BLACK);
    _sprite.drawFastHLine(0, 13, 160, 0x2945);
    _sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    _sprite.setTextSize(1);
    _sprite.setCursor(4, 3);
    _sprite.print(_getScreenTitle(_currentScreen));
    _drawSignalBars(_rssiToBars(state.wifiRssi, state.wifiConnected));
    _drawHeartbeat(state);
    _drawCloudIcon(state.cloudConnected);
}
// ------------------------------------

// --- Water Tank Drawings ----
void DisplayUI::_drawTankLevel(SystemState& state) {
    const uint8_t BAR_WIDTH     = 40;
    const uint8_t BAR_HEIGHT    = 110;
    const uint8_t BAR_X         = 2;
    const uint8_t BAR_Y         = 16;
    uint16_t barColor           = TFT_DARKGREY;
    // Tank outline
    _sprite.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_DARKGREY);
    // --- Tank level bar ---
    if (!state.tankStale) {
        uint16_t barHeight  = (state.tankLevelPct * BAR_HEIGHT) / 100;
        barColor            = state.tankLevelPct > 50 ? TFT_BLUE : state.tankLevelPct > 20 ? TFT_ORANGE : TFT_RED;
        _sprite.fillRect(
            BAR_X + 1, 
            BAR_Y + BAR_HEIGHT - barHeight, 
            BAR_WIDTH - 2, 
            barHeight, 
            barColor
        );
    }
    _sprite.setTextColor(TFT_WHITE);
    _sprite.setTextFont(2);
    _sprite.setTextSize(1);
    if (state.tankLevelPct == 100) _sprite.setCursor(5, 64);
    else if (state.tankLevelPct >= 10) _sprite.setCursor(9, 64);
    else _sprite.setCursor(14, 64);

    if (!state.tankStale) {
        _sprite.print(state.tankLevelPct);
        _sprite.print("%");
    }
    _sprite.setTextFont(1);
}
// ------------------------------------

// --- Power switch drawings ---
void DisplayUI::_drawPumpState(SystemState& state) {
    const int8_t  X       = 60;
    const int8_t  Y       = 33;
    const int8_t radius   = 15;

    _sprite.setTextFont(2);
    _sprite.setTextSize(1);
    _sprite.setCursor(X - 10, Y - 8);

    if (state.pumpState == PumpState::ON) {
        _sprite.fillCircle(X, Y, radius, TFT_GREEN);
        _sprite.print("ON");
    }else {
        _sprite.fillCircle(X, Y, radius, TFT_RED);
        _sprite.print("OFF");
    }
}

void DisplayUI::_drawPumpTimer(SystemState& state) {
    const int8_t  X       = 80;
    const int8_t  Y       = 15;

    _sprite.drawRect(X, Y, 80, 35, TFT_DARKGREY);
    // Draw the timer text
    _sprite.setTextColor(TFT_WHITE);
    _sprite.setCursor(X + 2, Y + 7);
    _sprite.setTextFont(1);
    _sprite.setTextSize(1);
    // _sprite.print("TIMER:");
    _sprite.setCursor(X + 23, Y + 2);
    _sprite.setTextFont(2);
    _sprite.setTextSize(1);
    _sprite.print("HH:MM");
    
    // Draw Interval
    _sprite.setTextFont(1);
    _sprite.setTextSize(1);
    _sprite.setCursor(X + 7, Y + 22);
    // _sprite.print("INT:");
    // _sprite.setCursor(X + 30, Y + 20);
    _sprite.print("HH:MM-HH:MM");
}

void DisplayUI::_drawHome(SystemState& state) {
    _sprite.fillSprite(TFT_BLACK);

    // --- Title bar ---
    _drawTitleBar(state);
    // --- Tank level ---
    _drawTankLevel(state);
    // --- Pump state ---
    _drawPumpState(state);
    // --- Pump timer ---
    _drawPumpTimer(state);

    // --- Pump state ---
    // _sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    // _sprite.setCursor(4, 20);
    // _sprite.print("Pump: ");
    // if (state.pumpState == PumpState::ON) {
    //     _sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    //     _sprite.print("ON");
    // } else {
    //     _sprite.setTextColor(TFT_RED, TFT_BLACK);
    //     _sprite.print("OFF");
    // }

    // --- Mode ---
    // _sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    // _sprite.setCursor(90, 20);
    // _sprite.print("Mode: ");
    // _sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    // _sprite.print(state.mode == OperatingMode::AUTO ? "AUTO" : "MAN");


    // --- Power readings ---
    // _sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    // _sprite.setCursor(4, 62);
    // _sprite.print("V:");
    // _sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    // _sprite.print(state.voltage, 1);
    // _sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    // _sprite.print("  A:");
    // _sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    // _sprite.print(state.current, 2);

    // _sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    // _sprite.setCursor(4, 75);
    // _sprite.print("W:");
    // _sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    // _sprite.print(state.powerWatts, 1);
    // _sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    // _sprite.print("  kWh:");
    // _sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    // _sprite.print(state.energyKwh, 3);

    // --- Fault banner ---
    // if (state.pumpFault || state.powerFault) {
    //     _sprite.fillRect(0, 108, 160, 20, TFT_RED);
    //     _sprite.setTextColor(TFT_WHITE, TFT_RED);
    //     _sprite.setCursor(20, 114);
    //     _sprite.print("!! FAULT DETECTED !!");
    // }

    // --- Uptime (bottom right) ---
    _sprite.setTextFont(1);
    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
    _sprite.setCursor(100, 118);
    uint32_t h = state.uptimeSeconds / 3600;
    uint32_t m = (state.uptimeSeconds % 3600) / 60;
    uint32_t s = state.uptimeSeconds % 60;
    char uptime[12];
    sprintf(uptime, "%02lu:%02lu:%02lu", h, m, s);
    _sprite.print(uptime);
}
