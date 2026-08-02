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

   _handleNavigation(state, event);
   _handleTimerEdit(state, event);
   _applyIdleTimeout();
   state.uiEditing = _editing();   // tells InputManager to keep off the pump

   // uptimeSeconds already forces a redraw once a second, so the blink needs a
   // force of its own to run at the display cadence instead.
   if (_editing()) _timerBlinkOn = !_timerBlinkOn;

   if (!state.hasChanged(Consumer::DISPLAY_CONSUMER) && !_screenChanged && !_editing())
        return;

    switch (_currentScreen) {
        case ScreenId::HOME: _drawHome(state); break;
    }

    _sprite.pushSprite(0, 0);
    state.markSeen(Consumer::DISPLAY_CONSUMER);
    _screenChanged = false;
}

// Owns the buttons whenever nothing is being edited: it moves the focus around
// the home screen and hands the timer over to the editor on SELECT.
void DisplayUI::_handleNavigation(SystemState& state, ButtonEvent event) {
    if (event == ButtonEvent::NONE || _editing()) return;

    switch (event) {
        case ButtonEvent::DOWN_PRESS:
            // Walks down the focusable widgets. The timer is the last one, so
            // stepping past it drops focus — append to FocusTarget to extend.
            _focus = (_focus == FocusTarget::NONE) ? FocusTarget::PUMP_TIMER
                                                   : FocusTarget::NONE;
            break;

        // Backing out. UP is the reverse of the walk, LEFT matches how LEFT
        // backs out of the first field once inside the editor.
        case ButtonEvent::UP_PRESS:
        case ButtonEvent::LEFT_PRESS:
            _focus = FocusTarget::NONE;
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
    if (_focus == FocusTarget::NONE && !_editing()) return;

    const uint32_t limit = _editing() ? UI_EDIT_TIMEOUT_MS : UI_FOCUS_TIMEOUT_MS;
    if (millis() - _lastInputMs < limit) return;

    // Same exit as backing out by hand: the edit is dropped, and a timer that is
    // already running is left to run.
    _timerField    = TimerField::NONE;
    _focus         = FocusTarget::NONE;
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

void DisplayUI::_goTo(ScreenId screen) {
    _currentScreen = screen;
    _screenChanged = true;
}

const char* DisplayUI::_getScreenTitle(ScreenId screen) {
    switch (screen) {
        case ScreenId::HOME:            return nullptr;
        default:                        return "";
    }
}
