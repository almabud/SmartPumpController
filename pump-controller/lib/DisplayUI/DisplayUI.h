#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../SystemState/SystemState.h"
#include "../InputManager/InputManager.h"


// Screens form a left/right strip with HOME at position 0. LEFT is always back
// one level and RIGHT always forward one, so a new page is added by appending
// here and giving it a case in the draw switch and the event routing.
enum class ScreenId : uint8_t {
    HOME,
    CONFIG,         // the settings list
    CONFIG_ITEM     // the editor for the row selected on CONFIG
};

// Which home-screen widget currently holds the buttons. Focus is a step short of
// editing: it only highlights. The order here is the order DOWN walks, so a new
// focusable widget is added by appending to it.
enum class FocusTarget : uint8_t {
    NONE,
    PUMP_TIMER,
    POWER_STATS,
    _COUNT          // keep last — the walk in _handleNavigation wraps on it
};

// The settings on the config page, in the order the list shows them. Adding one
// is this enum plus a row in CONFIG_DEFS (ConfigUI.cpp) — the list walk and the
// editor routing both work off the table.
enum class ConfigItem : uint8_t {
    TANK_FULL,
    TANK_EMPTY,
    BYPASS,
    _COUNT          // keep last — the list walk wraps on it
};

// How a setting is edited. METRES is a millimetre value shown as M.MMM and
// edited a digit at a time; BOOL is an on/off toggle.
enum class ConfigKind : uint8_t {
    METRES,
    BOOL
};

struct ConfigDef {
    const char* rowLabel;    // "Tank Full" — as it reads in the list
    const char* pageTitle;   // "TANK FULL" — as it reads in the editor title bar
    ConfigKind  kind;
};

// Defined in ConfigUI.cpp alongside the drawing that uses it.
const ConfigDef& configDef(uint8_t item);

// Number of editable digits in a METRES value: M.MMM, cursor 0 is the metres.
#define CFG_DIGITS 4

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

    // ---- Config page ----
    uint8_t  _cfgSel      = 0;      // selected row, and the item CONFIG_ITEM edits
    uint8_t  _cfgDigit    = 0;      // cursor on a METRES editor, 0 = the metres digit
    bool     _cfgBlinkOn  = true;
    uint16_t _cfgEditMm   = 0;      // working copies — state is written on commit only
    bool     _cfgEditBool = false;

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

    // The home screen's LEFT-hold shortcut — flips state.bypass and marks the
    // config dirty so ConfigStore persists it.
    void _toggleBypass(SystemState& state);

    void _handleNavigation(SystemState& state, ButtonEvent event);
    void _handleConfigList(SystemState& state, ButtonEvent event);
    void _handleConfigItem(SystemState& state, ButtonEvent event);
    void _beginConfigItem(SystemState& state);
    void _commitConfigItem(SystemState& state);
    void _adjustConfigDigit(int8_t dir);
    // Backs focus (and any edit) out once the buttons have been idle too long.
    void _applyIdleTimeout();
    void _goTo(ScreenId screen);
    // Leaves the config screens without committing. Used by the LEFT hold, the
    // idle timeout, and backing off the first digit.
    void _leaveConfig(ScreenId to);
    const char* _getScreenTitle(ScreenId screen);

    bool _onHome() const { return _currentScreen == ScreenId::HOME; }
    // True whenever the UI owns the buttons — a timer edit, or any screen past
    // home. InputManager stands down on this, so a SELECT hold inside a
    // settings page can never reach through to the pump.
    bool _uiOwnsButtons() const { return _editing() || !_onHome(); }

    // Boot screen drawing
    void _drawBoot(uint8_t step, uint8_t total, const char* label);
    // Home screen drawing
    void _drawHome(SystemState& state);
    // Config screen drawing
    void _drawConfig(SystemState& state);
    void _drawConfigItem(SystemState& state);
    // Label hard left, value hard right, over a highlight bar when selected.
    // _drawStatRow below is the black-background variant of the same idea.
    void _drawConfigRow(int16_t y, const char* label, const char* value,
                        bool selected, uint16_t valueColor);
    // Renders a millimetre value as "M.MMM", blinking the digit under the
    // cursor while `editing`. Returns nothing — it draws centred on its own.
    void _drawMetresValue(int16_t y, uint16_t mm, bool editing);
    // Title bar drawing
    void _drawTitleBar(SystemState& state);
    void _drawSignalBars(uint8_t level);
    uint8_t _rssiToBars(int8_t rssi, bool connected);
    void _drawHeartbeat(SystemState& state);
    void _drawCloudIcon(bool connected);
    // Dim when off, red when on — always drawn, so the bar never reflows.
    void _drawBypassIcon(bool on);
    // Tank level drawing
    void _drawTankLevel(SystemState& state);
    // Tank temperature drawing
    void _drawTankTemp(SystemState& state);
    // Pump state drawing
    void _drawPumpState(SystemState& state);
    // Timer drawing
    void _drawPumpTimer(SystemState& state);
    // Rolling 24h power stats box
    void _drawPowerStats(SystemState& state);
    // Label hard left, value hard right, inside a box. The caller sets the
    // font first, as with the timer helpers below.
    void _drawStatRow(int16_t boxX, int16_t boxW, int16_t y,
                      const char* label, const char* value, uint16_t valueColor);
    // Draws a zero-padded 2-digit field — or `label` when the field is still
    // unset — blinking it while it holds the cursor. Returns the x to continue
    // drawing at. The caller sets the font first.
    int16_t _drawTimerNum(int16_t x, int16_t y, uint8_t value,
                          TimerField field, const char* label, bool placeholder);
    int16_t _drawTimerText(int16_t x, int16_t y, const char* text, uint16_t color);
};
