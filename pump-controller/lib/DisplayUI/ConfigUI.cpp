#include "DisplayUI.h"
#include <TFT_eSPI.h>

extern TFT_eSprite _sprite;

// One row per setting, in list order. Adding a setting is an entry here plus a
// member on ConfigItem — neither the list walk nor the editor routing changes.
static const ConfigDef CONFIG_DEFS[] = {
    { "Tank Full",  "TANK FULL",  ConfigKind::METRES },
    { "Tank Empty", "TANK EMPTY", ConfigKind::METRES },
    { "Bypass",     "BYPASS",     ConfigKind::BOOL   }
};

static_assert(sizeof(CONFIG_DEFS) / sizeof(CONFIG_DEFS[0])
                  == static_cast<uint8_t>(ConfigItem::_COUNT),
              "CONFIG_DEFS and ConfigItem have drifted apart");

const ConfigDef& configDef(uint8_t item) {
    if (item >= static_cast<uint8_t>(ConfigItem::_COUNT)) item = 0;
    return CONFIG_DEFS[item];
}

// Millimetres as "M.MMM". The tank is calibrated in whole millimetres, so three
// decimals is the exact value rather than a rounding.
static void mmToMetres(uint16_t mm, char* out, size_t len) {
    snprintf(out, len, "%u.%03u m", mm / 1000, mm % 1000);
}

// ---- The list ------------------------------------------------------------

void DisplayUI::_drawConfigRow(int16_t y, const char* label, const char* value,
                               bool selected, uint16_t valueColor) {
    const int16_t PAD    = 5;
    const int16_t HEIGHT = 17;

    // The selected row gets the same grey the title bar uses, so the highlight
    // reads as part of the screen's furniture rather than a floating bar.
    uint16_t bg = selected ? 0x2945 : TFT_BLACK;
    if (selected) _sprite.fillRect(0, y - 1, 160, HEIGHT, bg);

    _sprite.setTextColor(selected ? TFT_WHITE : TFT_DARKGREY, bg);
    _sprite.setCursor(PAD, y);
    _sprite.print(label);

    _sprite.setTextColor(valueColor, bg);
    _sprite.setCursor(160 - PAD - _sprite.textWidth(value), y);
    _sprite.print(value);
}

void DisplayUI::_drawConfig(SystemState& state) {
    const int16_t FIRST_Y = 17;
    const int16_t PITCH   = 18;

    _sprite.fillSprite(TFT_BLACK);
    _drawTitleBar(state);

    _sprite.setTextFont(2);
    _sprite.setTextSize(1);

    char buf[12];

    for (uint8_t i = 0; i < static_cast<uint8_t>(ConfigItem::_COUNT); i++) {
        const char* value = buf;
        uint16_t    color = TFT_WHITE;

        switch (static_cast<ConfigItem>(i)) {
            case ConfigItem::TANK_FULL:
                mmToMetres(state.tankFullMm, buf, sizeof(buf));
                break;
            case ConfigItem::TANK_EMPTY:
                mmToMetres(state.tankEmptyMm, buf, sizeof(buf));
                break;
            case ConfigItem::BYPASS:
                value = state.bypass ? "ON" : "OFF";
                // Bypass on means the AUTO logic is standing down, which is a
                // state worth spotting from across the room.
                color = state.bypass ? TFT_GREEN : TFT_DARKGREY;
                break;
            default:
                continue;
        }

        _drawConfigRow(FIRST_Y + i * PITCH, CONFIG_DEFS[i].rowLabel, value,
                       i == _cfgSel, color);
    }

    _sprite.setTextFont(1);
}

// ---- The editor ----------------------------------------------------------

void DisplayUI::_drawMetresValue(int16_t y, uint16_t mm, bool editing) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%u.%03u", mm / 1000, mm % 1000);

    _sprite.setTextFont(4);
    _sprite.setTextSize(1);

    // Measured as one unit and centred, then drawn a character at a time so the
    // cursor digit can blink without the rest of the number moving.
    const int16_t unitW  = _sprite.textWidth(" m");
    int16_t       x      = (160 - (_sprite.textWidth(buf) + unitW)) / 2;
    uint8_t       digit  = 0;

    for (const char* c = buf; *c; c++) {
        char one[2] = { *c, '\0' };

        if (*c == '.') {
            _sprite.setTextColor(TFT_WHITE, TFT_BLACK);
            _sprite.drawString(one, x, y);
            x += _sprite.textWidth(one);
            continue;
        }

        // The cursor digit is the one that blinks; the rest stay solid so only
        // the cursor moves visually. Same model as _drawTimerNum().
        bool focused = editing && (digit == _cfgDigit);
        if (!focused || _cfgBlinkOn) {
            _sprite.setTextColor(focused ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
            _sprite.drawString(one, x, y);
        }
        x += _sprite.textWidth(one);
        digit++;
    }

    _sprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
    _sprite.drawString(" m", x, y);
}

void DisplayUI::_drawConfigItem(SystemState& state) {
    const int16_t VALUE_Y = 46;
    const int16_t HINT_Y  = 104;
    const int16_t HINT_Y2 = 114;

    _sprite.fillSprite(TFT_BLACK);
    _drawTitleBar(state);

    const ConfigDef& def = configDef(_cfgSel);

    if (def.kind == ConfigKind::BOOL) {
        const char* text = _cfgEditBool ? "ON" : "OFF";
        _sprite.setTextFont(4);
        _sprite.setTextSize(1);
        _sprite.setTextColor(_cfgEditBool ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
        _sprite.drawString(text, (160 - _sprite.textWidth(text)) / 2, VALUE_Y);
    } else {
        _drawMetresValue(VALUE_Y, _cfgEditMm, true);
    }

    // The key map is spelled out because these screens are reached rarely
    // enough that nobody will remember which button does what.
    _sprite.setTextFont(1);
    _sprite.setTextSize(1);
    _sprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
    _sprite.setCursor(5, HINT_Y);
    _sprite.print(def.kind == ConfigKind::BOOL ? "UP/DN toggle"
                                               : "UP/DN digit   L/R move");
    _sprite.setCursor(5, HINT_Y2);
    _sprite.print("SELECT save   LEFT back");
}
