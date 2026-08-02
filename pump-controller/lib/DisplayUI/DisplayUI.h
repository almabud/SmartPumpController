#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../SystemState/SystemState.h"
#include "../InputManager/InputManager.h"


enum class ScreenId : uint8_t {
    HOME
};

// Which home-screen widget currently holds the buttons. Focus is a step short of
// editing: it only highlights. The order here is the order DOWN walks, so a new
// focusable widget is added by appending to it.
enum class FocusTarget : uint8_t {
    NONE,
    PUMP_TIMER
};

// Cursor position while the pump timer is being edited. NONE = not editing.
enum class TimerField : uint8_t {
    NONE,
    TOTAL_HH, TOTAL_MM,     // row 1 — the whole run window
    BRK_HH,   BRK_MM,       // row 2, left  — how long the pump breaks for
    RUN_HH,   RUN_MM        // row 2, right — how long it runs between breaks
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

    // ---- Focus / idle ----
    FocusTarget _focus       = FocusTarget::NONE;
    uint32_t    _lastInputMs = 0;   // last button press, for the idle timeouts

    // ---- Pump timer editing ----
    TimerField _timerField   = TimerField::NONE;
    bool       _timerBlinkOn = true;
    bool       _useDutyCycle = false;   // true once the cursor has reached row 2
    // One bit per TimerField. An untouched field draws its "HH"/"MM" label
    // instead of its value, so a field left at zero still reads as unset.
    uint8_t    _timerTouched = 0;
    // Working copies — the state fields are only written on commit.
    uint8_t    _editTotalH = 0, _editTotalM = 0;
    uint8_t    _editBrkH   = 0, _editBrkM   = 0;
    uint8_t    _editRunH   = 0, _editRunM   = 0;

    void _handleTimerEdit(SystemState& state, ButtonEvent event);
    void _beginTimerEdit(SystemState& state);
    void _commitTimer(SystemState& state);
    void _adjustTimerField(int8_t dir);
    void _resetDutyRow();
    static bool _dutyValid(uint32_t total, uint32_t brk, uint32_t run);
    bool _editing() const { return _timerField != TimerField::NONE; }

    static uint8_t _bit(TimerField f) { return 1u << static_cast<uint8_t>(f); }
    bool _touched(TimerField f) const { return _timerTouched & _bit(f); }
    // A field left alone shows its label only while its whole row is untouched;
    // once the row is in use the rest of it reads as the 00 it will commit as.
    bool _row1Untouched() const {
        return !_touched(TimerField::TOTAL_HH) && !_touched(TimerField::TOTAL_MM);
    }
    bool _row2Untouched() const {
        return !_touched(TimerField::BRK_HH) && !_touched(TimerField::BRK_MM)
            && !_touched(TimerField::RUN_HH) && !_touched(TimerField::RUN_MM);
    }

    void _handleNavigation(SystemState& state, ButtonEvent event);
    // Backs focus (and any edit) out once the buttons have been idle too long.
    void _applyIdleTimeout();
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
    // Draws a zero-padded 2-digit field — or `label` when the field is still
    // unset — blinking it while it holds the cursor. Returns the x to continue
    // drawing at. The caller sets the font first.
    int16_t _drawTimerNum(int16_t x, int16_t y, uint8_t value,
                          TimerField field, const char* label, bool placeholder);
    int16_t _drawTimerText(int16_t x, int16_t y, const char* text, uint16_t color);
};
