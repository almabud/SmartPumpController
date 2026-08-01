#include "DisplayUI.h"
#include <TFT_eSPI.h>
#include "config.h"
#include "FontAwesomesolid9006.h"

extern TFT_eSprite _sprite;

// --- Boot screen ---
// Drawn from setup() only, so it bypasses the change-detection gate in
// update() and pushes the sprite itself.
void DisplayUI::_drawBoot(uint8_t step, uint8_t total, const char* label) {
    const int16_t CENTER_X  = 80;
    const int16_t ICON_Y    = 6;
    const int16_t TITLE1_Y  = 26;
    const int16_t TITLE2_Y  = 58;
    const int16_t BAR_X     = 20;
    const int16_t BAR_Y     = 92;
    const int16_t BAR_WIDTH = 120;
    const int16_t BAR_HEIGHT= 6;
    const int16_t STATUS_Y  = 104;
    const int16_t VERSION_Y = 118;

    _sprite.fillSprite(TFT_BLACK);

    // --- Droplet icon ---
    _sprite.loadFont(FontAwesomesolid9006);
    _sprite.setTextColor(TFT_CYAN, TFT_BLACK);
    _sprite.drawString("", CENTER_X - 4, ICON_Y);   // glyph is 9px wide
    _sprite.unloadFont();

    // --- Title ---
    // Centred by datum rather than hand-measured cursors: both fonts are
    // proportional, so the widths are not worth hardcoding.
    // Font 4 is the largest face available (6 and 7 are digits-only), and it
    // cannot hold the full name: "SMART PUMP" measures 160px and
    // "CONTROLLER" 164px against a 160px screen. So the hero word is font 4
    // and the full product name sits below it in font 2 (118px).
    _sprite.setTextDatum(TC_DATUM);
    _sprite.setTextColor(TFT_CYAN, TFT_BLACK);
    _sprite.drawString("SMART", CENTER_X, TITLE1_Y, 4);
    _sprite.drawString("PUMP CONTROLLER", CENTER_X, TITLE2_Y, 2);

    // --- Progress bar ---
    if (step > total) step = total;
    _sprite.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, TFT_DARKGREY);
    if (total > 0) {
        int16_t fillWidth = ((BAR_WIDTH - 2) * step) / total;
        _sprite.fillRect(BAR_X + 1, BAR_Y + 1, fillWidth, BAR_HEIGHT - 2, TFT_CYAN);
    }

    // --- Status line ---
    _sprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
    _sprite.drawString(label, CENTER_X, STATUS_Y, 1);

    // --- Version ---
    _sprite.setTextDatum(TR_DATUM);
    _sprite.drawString(FIRMWARE_VERSION, 156, VERSION_Y, 1);

    // Restore the defaults the rest of the UI assumes
    _sprite.setTextDatum(TL_DATUM);
    _sprite.setTextFont(1);
}

void DisplayUI::showBoot(uint8_t step, uint8_t total, const char* label) {
    _drawBoot(step, total, label);
    _sprite.pushSprite(0, 0);
    // delay() is confined to setup(); the loop() scheduler stays non-blocking.
    delay(BOOT_STEP_MIN_MS);
}
