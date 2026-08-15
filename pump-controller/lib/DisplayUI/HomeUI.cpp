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

void DisplayUI::_drawTankTemp(SystemState& state) {
    if (state.tankStale) return;

    const int16_t CENTER_X = 22;   // BAR_X + BAR_WIDTH / 2
    const int16_t Y        = 84;   // just below the level % (font 2 at y = 64)

    char buf[8];
    snprintf(buf, sizeof(buf), "%.1f", state.tankTempC);

    _sprite.setTextFont(1);
    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_WHITE);

    // Value + degree mark + "C", centered as one unit.
    // Font 1 has no degree glyph, so the ring is drawn as a primitive.
    int16_t valueWidth = _sprite.textWidth(buf);
    int16_t totalWidth = valueWidth + 3 + _sprite.textWidth("C");
    int16_t x          = CENTER_X - totalWidth / 2;

    _sprite.setCursor(x, Y);
    _sprite.print(buf);
    _sprite.drawCircle(x + valueWidth + 1, Y + 1, 1, TFT_WHITE);
    _sprite.setCursor(x + valueWidth + 4, Y);
    _sprite.print("C");
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

int16_t DisplayUI::_drawTimerNum(int16_t x, int16_t y, uint8_t value,
                                 TimerField field, const char* label, bool placeholder) {
    char buf[4];
    if (placeholder) snprintf(buf, sizeof(buf), "%s", label);
    else             snprintf(buf, sizeof(buf), "%02u", value);

    bool focused = (_timerField == field);
    // The focused field is the one that blinks; the rest stay solid so only the
    // cursor moves visually.
    if (!focused || _timerBlinkOn) {
        uint16_t color = focused     ? TFT_YELLOW
                       : placeholder ? TFT_DARKGREY   // reads as "not set yet"
                                     : TFT_WHITE;
        _sprite.setTextColor(color, TFT_BLACK);
        _sprite.setCursor(x, y);
        _sprite.print(buf);
    }
    return x + _sprite.textWidth(buf);
}

int16_t DisplayUI::_drawTimerText(int16_t x, int16_t y, const char* text, uint16_t color) {
    _sprite.setTextColor(color, TFT_BLACK);
    _sprite.setCursor(x, y);
    _sprite.print(text);
    return x + _sprite.textWidth(text);
}

void DisplayUI::_drawPumpTimer(SystemState& state) {
    const int8_t X     = 80;
    const int8_t Y     = 15;
    const int8_t WIDTH = 80;

    const bool editing  = _editing();
    const bool focused  = (_focus == FocusTarget::PUMP_TIMER);
    const bool running  = (state.timerPhase != TimerPhase::IDLE);
    const bool breaking = (state.timerPhase == TimerPhase::BREAKING);

    // A yellow border is the cue that the box has the buttons, focused or being
    // edited. The blinking field cursor is what tells the two apart.
    _sprite.drawRect(X, Y, WIDTH, 35, (editing || focused) ? TFT_YELLOW
                                                           : TFT_DARKGREY);

    // --- Row 1: the whole run window ---
    // A running window also shows seconds, so the row is measured and centred
    // rather than started at a fixed x — HH:MM:SS would not fit otherwise.
    _sprite.setTextFont(2);
    _sprite.setTextSize(1);

    // Labels only stand in while the whole row is untouched; the moment one
    // field is set the others read as the 00 they will commit as.
    const bool row1Labels = _row1Untouched();
    const bool row2Labels = _row2Untouched();

    char hh[4], mm[4], row1[12];
    if (editing) {
        snprintf(hh, sizeof(hh), row1Labels ? "HH" : "%02u", _editTotalH);
        snprintf(mm, sizeof(mm), row1Labels ? "MM" : "%02u", _editTotalM);
        snprintf(row1, sizeof(row1), "%s:%s", hh, mm);
    } else if (running) {
        // The run budget is parked during a break, so a frozen number would
        // read as a hang — show the break counting down instead, in orange.
        uint32_t shown = breaking ? state.timerPhaseSec : state.timerRemainSec;
        snprintf(row1, sizeof(row1), "%02lu:%02lu:%02lu",
                 (unsigned long)(shown / 3600),
                 (unsigned long)((shown % 3600) / 60),
                 (unsigned long)(shown % 60));
    } else {
        snprintf(row1, sizeof(row1), "HH:MM");
    }

    int16_t x = X + (WIDTH - _sprite.textWidth(row1)) / 2;
    int16_t y = Y + 2;

    if (editing) {
        x = _drawTimerNum(x, y, _editTotalH, TimerField::TOTAL_HH, "HH", row1Labels);
        x = _drawTimerText(x, y, ":", TFT_WHITE);
        x = _drawTimerNum(x, y, _editTotalM, TimerField::TOTAL_MM, "MM", row1Labels);
    } else {
        _drawTimerText(x, y, row1, !running  ? TFT_DARKGREY
                                  : breaking ? TFT_ORANGE
                                             : TFT_WHITE);
    }

    // --- Row 2: the RUN-BREAK duty cycle ---
    _sprite.setTextFont(1);
    _sprite.setTextSize(1);
    x = X + 7;
    y = Y + 22;

    // Shown dim and unfilled unless a duty cycle actually exists, so a timer
    // committed straight from row 1 still reads as "runs continuously".
    bool haveDuty = editing ? _useDutyCycle
                            : (running && state.timerBreakSec > 0 && state.timerRunSec > 0);
    if (!haveDuty) {
        _drawTimerText(x, y, "HH:MM-HH:MM", TFT_DARKGREY);
        return;
    }

    // Left pair is the break, right pair is the run it follows.
    uint8_t brkH = _editBrkH, brkM = _editBrkM, runH = _editRunH, runM = _editRunM;
    bool    labels = row2Labels;
    if (!editing) {
        brkH = state.timerBreakSec / 3600;  brkM = (state.timerBreakSec % 3600) / 60;
        runH = state.timerRunSec   / 3600;  runM = (state.timerRunSec   % 3600) / 60;
        labels = false;
    }

    x = _drawTimerNum(x, y, brkH, TimerField::BRK_HH, "HH", labels);
    x = _drawTimerText(x, y, ":", TFT_WHITE);
    x = _drawTimerNum(x, y, brkM, TimerField::BRK_MM, "MM", labels);
    x = _drawTimerText(x, y, "-", TFT_WHITE);
    x = _drawTimerNum(x, y, runH, TimerField::RUN_HH, "HH", labels);
    x = _drawTimerText(x, y, ":", TFT_WHITE);
    x = _drawTimerNum(x, y, runM, TimerField::RUN_MM, "MM", labels);
}

void DisplayUI::_drawStatRow(int16_t boxX, int16_t boxW, int16_t y,
                             const char* label, const char* value, uint16_t valueColor) {
    const int16_t PAD = 4;

    // The label is dim and the value bright: at this size the eye should land
    // on the number, not on the word telling it which number it is.
    _sprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
    _sprite.setCursor(boxX + PAD, y);
    _sprite.print(label);

    _sprite.setTextColor(valueColor, TFT_BLACK);
    _sprite.setCursor(boxX + boxW - PAD - _sprite.textWidth(value), y);
    _sprite.print(value);
}

void DisplayUI::_drawPowerStats(SystemState& state) {
    const int16_t X = 46, Y = 52, W = 114, H = 62;

    _sprite.drawRect(X, Y, W, H, TFT_DARKGREY);

    // Title strip in the same grey as the screen's top bar, so the box reads as
    // part of the same furniture rather than a floating panel.
    _sprite.fillRect(X + 1, Y + 1, W - 2, 11, 0x2945);

    _sprite.setTextFont(1);
    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_WHITE, 0x2945);
    _sprite.setCursor(X + 4, Y + 3);
    _sprite.print("Last 24h");

    char buf[20];

    // HH:MM:SS rather than HH:MM — it matches the uptime readout below, and a
    // short test run would otherwise round away to 00:00 and look broken.
    uint32_t secs = state.stats24hRuntimeSec;
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
             (unsigned long)(secs / 3600),
             (unsigned long)((secs % 3600) / 60),
             (unsigned long)(secs % 60));
    _drawStatRow(X, W, Y + 15, "Run", buf, TFT_WHITE);

    snprintf(buf, sizeof(buf), "%u", state.stats24hCycles);
    _drawStatRow(X, W, Y + 27, "Cyc", buf, TFT_WHITE);

    // This box is the only place power health is visible now that the old
    // full-width fault banner would collide with it, so the fault has to show
    // here or nowhere.
    snprintf(buf, sizeof(buf), "%.3f", state.stats24hEnergyKwh);
    _drawStatRow(X, W, Y + 39, "kWh", buf, state.powerFault ? TFT_RED : TFT_WHITE);

    // The bottom row swaps with the pump. Live readings only mean anything
    // while it runs; the 24h averages are what you study once it has stopped.
    if (state.pumpState == PumpState::ON) {
        snprintf(buf, sizeof(buf), "%.1fA  %.0fW", state.current, state.powerWatts);
        _sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    } else {
        snprintf(buf, sizeof(buf), "avg %.1fA pk %.1fA",
                 state.stats24hAvgCurrent, state.stats24hPeakCurrent);
        _sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    _sprite.setCursor(X + 4, Y + 51);
    _sprite.print(buf);
}

void DisplayUI::_drawHome(SystemState& state) {
    _sprite.fillSprite(TFT_BLACK);

    // --- Title bar ---
    _drawTitleBar(state);
    // --- Tank level ---
    _drawTankLevel(state);
    // --- Tank temperature ---
    _drawTankTemp(state);
    // --- Pump state ---
    _drawPumpState(state);
    // --- Pump timer ---
    _drawPumpTimer(state);
    // --- Rolling 24h power stats ---
    _drawPowerStats(state);

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
