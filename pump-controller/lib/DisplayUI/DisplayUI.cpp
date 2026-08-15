#include "DisplayUI.h"
#include "config.h"

TFT_eSPI    _tft;
TFT_eSprite _sprite(&_tft);

void DisplayUI::begin() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    _tft.init();
    _tft.setRotation(-1);
    _tft.fillScreen(TFT_BLACK);

    _sprite.setColorDepth(16);
    _sprite.createSprite(160, 128);
    _sprite.setSwapBytes(true);

    Serial.println("[DisplayUI] begin - TFT_eSPI ready");
}

void DisplayUI::update(SystemState& state, ButtonEvent event) {
   if (event != ButtonEvent::NONE) _lastInputMs = millis();

   // The one global gesture, handled before the per-screen routing so it works
   // from any depth. On home there is nowhere further back, so it reads as the
   // short LEFT it would otherwise have been.
   if (event == ButtonEvent::LEFT_LONG_PRESS) {
       if (_onHome()) _focus = FocusTarget::NONE;
       else           _leaveConfig(ScreenId::HOME);
       _screenChanged = true;
       event          = ButtonEvent::NONE;
   }

   switch (_currentScreen) {
       case ScreenId::HOME:
           _handleNavigation(state, event);
           _handleTimerEdit(state, event);
           break;
       case ScreenId::CONFIG:      _handleConfigList(state, event); break;
       case ScreenId::CONFIG_ITEM: _handleConfigItem(state, event); break;
   }

   _applyIdleTimeout();
   state.uiEditing = _uiOwnsButtons();   // tells InputManager to keep off the pump

   // uptimeSeconds already forces a redraw once a second, so the blinks need a
   // force of their own to run at the display cadence instead.
   const bool blinking = _editing() || _currentScreen == ScreenId::CONFIG_ITEM;
   if (_editing()) _timerBlinkOn = !_timerBlinkOn;
   if (_currentScreen == ScreenId::CONFIG_ITEM) _cfgBlinkOn = !_cfgBlinkOn;

   if (!state.hasChanged(Consumer::DISPLAY_CONSUMER) && !_screenChanged && !blinking)
        return;

    switch (_currentScreen) {
        case ScreenId::HOME:        _drawHome(state);       break;
        case ScreenId::CONFIG:      _drawConfig(state);     break;
        case ScreenId::CONFIG_ITEM: _drawConfigItem(state); break;
    }

    _sprite.pushSprite(0, 0);
    state.markSeen(Consumer::DISPLAY_CONSUMER);
    _screenChanged = false;
}

// Owns the buttons whenever nothing is being edited: it moves the focus around
// the home screen and hands the timer over to the editor on SELECT.
void DisplayUI::_handleNavigation(SystemState& state, ButtonEvent event) {
    if (event == ButtonEvent::NONE || _editing()) return;

    // NONE is part of the cycle rather than an escape from it: walking off the
    // end of the widgets lands back on an unfocused screen, so a full circuit
    // always returns to where it started.
    const uint8_t count = static_cast<uint8_t>(FocusTarget::_COUNT);
    const uint8_t at    = static_cast<uint8_t>(_focus);

    switch (event) {
        case ButtonEvent::DOWN_PRESS:
            // Walks the focusable widgets in FocusTarget order — a new one is
            // added by appending to the enum, nothing here changes.
            _focus = static_cast<FocusTarget>((at + 1) % count);
            break;

        case ButtonEvent::UP_PRESS:
            // The reverse walk. With more than one widget this has to step back
            // one rather than drop straight out, or the last widget in the list
            // is unreachable without cycling all the way round.
            _focus = static_cast<FocusTarget>((at + count - 1) % count);
            break;

        case ButtonEvent::LEFT_PRESS:
            // Still the escape hatch, matching how LEFT backs out of the first
            // field once inside the editor. Home is position 0 in the screen
            // strip, so there is nothing further left to go to.
            _focus = FocusTarget::NONE;
            break;

        case ButtonEvent::RIGHT_PRESS:
            // Forward one level in the screen strip. The focus goes with it, so
            // coming back never lands on a stale highlight.
            _focus  = FocusTarget::NONE;
            _cfgSel = 0;
            _goTo(ScreenId::CONFIG);
            break;

        case ButtonEvent::SELECT_PRESS:
            // Focus is only a highlight; SELECT is what commits to editing.
            if (_focus == FocusTarget::PUMP_TIMER) _beginTimerEdit(state);
            break;

        default:
            return;   // nothing moved, so nothing to redraw
    }

    _screenChanged = true;
}

void DisplayUI::_applyIdleTimeout() {
    if (_onHome() && _focus == FocusTarget::NONE && !_editing()) return;

    // Off home the long window applies to the list as well as the editor —
    // reading a settings list is not a ten-second activity.
    const uint32_t limit = (_editing() || !_onHome()) ? UI_EDIT_TIMEOUT_MS
                                                     : UI_FOCUS_TIMEOUT_MS;
    if (millis() - _lastInputMs < limit) return;

    // Same exit as backing out by hand: the edit is dropped, and a timer that is
    // already running is left to run.
    _timerField    = TimerField::NONE;
    _focus         = FocusTarget::NONE;
    if (!_onHome()) _leaveConfig(ScreenId::HOME);
    _screenChanged = true;
}

// ---- Pump timer editing --------------------------------------------------

// Steps the focused field by one increment in `dir`, wrapping at its limit.
void DisplayUI::_adjustTimerField(int8_t dir) {
    uint8_t* field = nullptr;
    bool     hours = false;

    switch (_timerField) {
        case TimerField::TOTAL_HH: field = &_editTotalH; hours = true;  break;
        case TimerField::TOTAL_MM: field = &_editTotalM;                break;
        case TimerField::BRK_HH:   field = &_editBrkH;   hours = true;  break;
        case TimerField::BRK_MM:   field = &_editBrkM;                  break;
        case TimerField::RUN_HH:   field = &_editRunH;   hours = true;  break;
        case TimerField::RUN_MM:   field = &_editRunM;                  break;
        default: return;
    }

    bool wasUnset  = !_touched(_timerField);
    _timerTouched |= _bit(_timerField);   // it now holds a value the user chose

    // Stepping down out of an unset field lands on 00 rather than wrapping to
    // the top — down off "nothing" reads as a reset, not as 23.
    if (wasUnset && dir < 0) {
        *field = 0;
        return;
    }

    if (hours) {
        // 0..TIMER_MAX_HOURS, wrapping both ways
        if (dir > 0) *field = (*field >= TIMER_MAX_HOURS) ? 0 : *field + 1;
        else         *field = (*field == 0) ? TIMER_MAX_HOURS : *field - 1;
    } else {
        const uint8_t lastStep = 60 - TIMER_MINUTE_STEP;   // e.g. 55 at a step of 5
        if (dir > 0) *field = (*field >= lastStep) ? 0 : *field + TIMER_MINUTE_STEP;
        else         *field = (*field == 0) ? lastStep : *field - TIMER_MINUTE_STEP;
    }
}

// Deliberately permissive: the cycle does not have to divide the window, and a
// final slice cut short by the window ending is normal. Only settings that
// could never produce a break at all are rejected. Both fields blank is fine —
// that just means no duty cycle.
bool DisplayUI::_dutyValid(uint32_t total, uint32_t brk, uint32_t run) {
    if (brk == 0 && run == 0) return true;
    if (brk == 0 || run == 0) return false;   // half a duty cycle is not one
    if (run >= total)         return false;   // the first break never arrives
    return true;
}

void DisplayUI::_resetDutyRow() {
    _editBrkH = _editBrkM = _editRunH = _editRunM = 0;
    _timerTouched &= ~(_bit(TimerField::BRK_HH) | _bit(TimerField::BRK_MM)
                     | _bit(TimerField::RUN_HH) | _bit(TimerField::RUN_MM));
}

void DisplayUI::_commitTimer(SystemState& state) {
    uint32_t total = _editTotalH * 3600UL + _editTotalM * 60UL;
    if (total == 0) return;         // an empty window is not a timer — stay in edit

    uint32_t brk = _useDutyCycle ? (_editBrkH * 3600UL + _editBrkM * 60UL) : 0;
    uint32_t run = _useDutyCycle ? (_editRunH * 3600UL + _editRunM * 60UL) : 0;

    if (!_dutyValid(total, brk, run)) {
        // Hand row 2 back blank rather than starting something nonsensical.
        _resetDutyRow();
        _useDutyCycle = true;
        _timerField   = TimerField::BRK_HH;
        Serial.printf("[DisplayUI] duty cycle rejected - break %lus after %lus of running, %lus window\n",
                      (unsigned long)brk, (unsigned long)run, (unsigned long)total);
        return;
    }

    state.timerTotalSec = total;
    state.timerBreakSec = brk;
    state.timerRunSec   = run;
    state.timerRequest  = TimerRequest::START;

    _timerField = TimerField::NONE;
    _focus      = FocusTarget::NONE;   // the timer is armed, the widget is done
    Serial.printf("[DisplayUI] timer set - %lus window, break %lus after every %lus of running\n",
                  (unsigned long)total, (unsigned long)brk, (unsigned long)run);
}

// Entered from the focused timer with SELECT — see _handleNavigation().
void DisplayUI::_beginTimerEdit(SystemState& state) {
    // Preload whatever is already armed so an edit is a tweak, not a retype.
    _editTotalH   = state.timerTotalSec / 3600;
    _editTotalM   = (state.timerTotalSec % 3600) / 60;
    _editBrkH     = state.timerBreakSec    / 3600;
    _editBrkM     = (state.timerBreakSec   % 3600) / 60;
    _editRunH     = state.timerRunSec      / 3600;
    _editRunM     = (state.timerRunSec     % 3600) / 60;
    _useDutyCycle = (state.timerBreakSec > 0 && state.timerRunSec > 0);

    // Preloaded values are already the user's, so they show as digits;
    // anything not preloaded starts on its label.
    _timerTouched = 0;
    if (state.timerTotalSec > 0) {
        _timerTouched |= _bit(TimerField::TOTAL_HH) | _bit(TimerField::TOTAL_MM);
    }
    if (_useDutyCycle) {
        _timerTouched |= _bit(TimerField::BRK_HH) | _bit(TimerField::BRK_MM)
                       | _bit(TimerField::RUN_HH) | _bit(TimerField::RUN_MM);
    }

    _timerField   = TimerField::TOTAL_HH;
    _timerBlinkOn = false;   // update() flips it, so the field shows first
}

void DisplayUI::_handleTimerEdit(SystemState& state, ButtonEvent event) {
    if (event == ButtonEvent::NONE || !_editing()) return;

    switch (event) {
        case ButtonEvent::UP_PRESS:
            _adjustTimerField(+1);
            break;

        case ButtonEvent::DOWN_PRESS:
            _adjustTimerField(-1);
            break;

        case ButtonEvent::LEFT_PRESS:
            switch (_timerField) {
                // Backing off the first field leaves edit mode, and leaves the
                // widget behind with it. Only the edit is discarded — a timer
                // already running is left alone.
                case TimerField::TOTAL_HH:
                    _timerField = TimerField::NONE;
                    _focus      = FocusTarget::NONE;
                    break;
                case TimerField::TOTAL_MM: _timerField = TimerField::TOTAL_HH; break;
                case TimerField::BRK_HH:   _timerField = TimerField::TOTAL_MM; break;
                case TimerField::BRK_MM:   _timerField = TimerField::BRK_HH;   break;
                case TimerField::RUN_HH:   _timerField = TimerField::BRK_MM;   break;
                case TimerField::RUN_MM:   _timerField = TimerField::RUN_HH;   break;
                default: break;
            }
            break;

        case ButtonEvent::RIGHT_PRESS:
            switch (_timerField) {
                case TimerField::TOTAL_HH: _timerField = TimerField::TOTAL_MM; break;
                // Walking right off row 1 is what opens the duty-cycle row —
                // reaching it changes nothing on its own, SELECT still starts.
                case TimerField::TOTAL_MM:
                    _useDutyCycle = true;
                    _timerField   = TimerField::BRK_HH;
                    break;
                case TimerField::BRK_HH:   _timerField = TimerField::BRK_MM;   break;
                case TimerField::BRK_MM:   _timerField = TimerField::RUN_HH;   break;
                case TimerField::RUN_HH:   _timerField = TimerField::RUN_MM;   break;
                default: break;   // RUN_MM is the last field
            }
            break;

        case ButtonEvent::SELECT_PRESS:
            _commitTimer(state);
            break;

        case ButtonEvent::SELECT_LONG_PRESS:
            // Same as backing out with LEFT: drop the edit, leave any running
            // timer running. InputManager stands down while uiEditing is set.
            _timerField = TimerField::NONE;
            _focus      = FocusTarget::NONE;
            break;

        default:
            break;
    }
}

// ---- Config page ---------------------------------------------------------

// Owns the buttons on the settings list: walks the rows and hands one over to
// the editor on SELECT.
void DisplayUI::_handleConfigList(SystemState& state, ButtonEvent event) {
    if (event == ButtonEvent::NONE) return;

    const uint8_t count = static_cast<uint8_t>(ConfigItem::_COUNT);

    switch (event) {
        // Unlike the home screen's walk there is no NONE position here: a list
        // row is always selected, so the wrap is a plain modulo over the rows.
        case ButtonEvent::DOWN_PRESS:
            _cfgSel = (_cfgSel + 1) % count;
            break;

        case ButtonEvent::UP_PRESS:
            _cfgSel = (_cfgSel + count - 1) % count;
            break;

        case ButtonEvent::LEFT_PRESS:
            _leaveConfig(ScreenId::HOME);
            break;

        case ButtonEvent::RIGHT_PRESS:
            // Forward one level in the screen strip. There is no page past this
            // one yet, so the press is deliberately inert rather than wrapping.
            return;

        case ButtonEvent::SELECT_PRESS:
            _beginConfigItem(state);
            break;

        default:
            return;   // nothing moved, so nothing to redraw
    }

    _screenChanged = true;
}

// Loads the working copy so an edit is a tweak rather than a retype — the same
// preload _beginTimerEdit does.
void DisplayUI::_beginConfigItem(SystemState& state) {
    switch (static_cast<ConfigItem>(_cfgSel)) {
        case ConfigItem::TANK_FULL:  _cfgEditMm   = state.tankFullMm;  break;
        case ConfigItem::TANK_EMPTY: _cfgEditMm   = state.tankEmptyMm; break;
        case ConfigItem::BYPASS:     _cfgEditBool = state.bypass;      break;
        default: return;
    }

    _cfgDigit   = 0;
    _cfgBlinkOn = false;   // update() flips it, so the digit shows first
    _goTo(ScreenId::CONFIG_ITEM);
}

// Steps the digit under the cursor by one, wrapping 0-9. Out-of-range values
// are reachable mid-edit on purpose: the check belongs on commit, the same
// split _dutyValid() uses for the timer.
void DisplayUI::_adjustConfigDigit(int8_t dir) {
    uint16_t place = 1;
    for (uint8_t i = _cfgDigit + 1; i < CFG_DIGITS; i++) place *= 10;

    uint8_t was = (_cfgEditMm / place) % 10;
    uint8_t now = (was + dir + 10) % 10;

    _cfgEditMm = _cfgEditMm + (uint16_t)(now * place) - (uint16_t)(was * place);
}

void DisplayUI::_commitConfigItem(SystemState& state) {
    const ConfigItem item = static_cast<ConfigItem>(_cfgSel);

    if (item == ConfigItem::BYPASS) {
        state.bypass = _cfgEditBool;
    } else {
        // Both distances are validated as a pair against the other one as it
        // currently stands, because it is the span between them that has to
        // make sense — a value fine on its own can still invert the scale.
        const bool full     = (item == ConfigItem::TANK_FULL);
        const uint16_t other = full ? state.tankEmptyMm : state.tankFullMm;

        bool ok = _cfgEditMm >= TANK_MIN_MM && _cfgEditMm <= TANK_MAX_MM
               && (full ? (other > _cfgEditMm) : (_cfgEditMm > other));

        if (!ok) {
            // Hand the editor back with the cursor reset rather than storing a
            // calibration that would make every level reading nonsense.
            _cfgDigit = 0;
            Serial.printf("[DisplayUI] %s rejected - %umm (valid %d-%dmm, "
                          "empty must exceed full, other side is %umm)\n",
                          configDef(_cfgSel).pageTitle, _cfgEditMm,
                          TANK_MIN_MM, TANK_MAX_MM, other);
            return;
        }

        if (full) state.tankFullMm  = _cfgEditMm;
        else      state.tankEmptyMm = _cfgEditMm;
    }

    // ConfigStore picks this up on the same loop pass and writes it to NVS.
    state.configDirty = true;
    Serial.printf("[DisplayUI] %s set\n", configDef(_cfgSel).pageTitle);

    _leaveConfig(ScreenId::CONFIG);
}

void DisplayUI::_handleConfigItem(SystemState& state, ButtonEvent event) {
    if (event == ButtonEvent::NONE) return;

    const bool isBool = (configDef(_cfgSel).kind == ConfigKind::BOOL);

    switch (event) {
        case ButtonEvent::UP_PRESS:
            if (isBool) _cfgEditBool = !_cfgEditBool;
            else        _adjustConfigDigit(+1);
            break;

        case ButtonEvent::DOWN_PRESS:
            if (isBool) _cfgEditBool = !_cfgEditBool;
            else        _adjustConfigDigit(-1);
            break;

        case ButtonEvent::LEFT_PRESS:
            // Backing off the first digit leaves the editor, discarding — the
            // same exit LEFT gives off the timer's first field.
            if (isBool || _cfgDigit == 0) _leaveConfig(ScreenId::CONFIG);
            else                          _cfgDigit--;
            break;

        case ButtonEvent::RIGHT_PRESS:
            if (!isBool && _cfgDigit < CFG_DIGITS - 1) _cfgDigit++;
            break;

        case ButtonEvent::SELECT_PRESS:
            _commitConfigItem(state);
            break;

        case ButtonEvent::SELECT_LONG_PRESS:
            // Same as backing out with LEFT. InputManager stands down while
            // uiEditing is set, so the pump is not touched.
            _leaveConfig(ScreenId::CONFIG);
            break;

        default:
            return;
    }

    _screenChanged = true;
}

void DisplayUI::_goTo(ScreenId screen) {
    _currentScreen = screen;
    _screenChanged = true;
}

// Every way out of the config screens that is not a commit. The working copies
// are simply abandoned — nothing has been written to state yet.
void DisplayUI::_leaveConfig(ScreenId to) {
    _cfgDigit = 0;
    if (to == ScreenId::HOME) _cfgSel = 0;
    _goTo(to);
}

const char* DisplayUI::_getScreenTitle(ScreenId screen) {
    switch (screen) {
        // Home carries its own furniture and wants the space, so the title slot
        // is left blank rather than unset — the title bar prints it either way.
        case ScreenId::HOME:        return "";
        case ScreenId::CONFIG:      return "CONFIG";
        case ScreenId::CONFIG_ITEM: return configDef(_cfgSel).pageTitle;
        default:                    return "";
    }
}
