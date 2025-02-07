#include "display.h"

#define TFT_CS A5
#define TFT_RST A4
#define TFT_DC A3
// Color definitions for ST7735 display
// #define ST7735_BLACK      0x0000  // 0,     0,   0
// #define ST7735_WHITE      0xFFFF  // 255, 255, 255
// #define ST7735_RED        0xF800  // 255,   0,   0
// #define ST7735_GREEN      0x07E0  //   0, 255,   0
// #define ST7735_BLUE       0x001F  //   0,   0, 255
// #define ST7735_YELLOW     0xFFE0  // 255, 255,   0
#define ST77XX_DARKGREY 0x7BEF  // 123, 123, 123

// Initialize display module.
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Animation constants.
const uint8_t WAVE_LAYERS = 3;
const uint8_t BUBBLE_COUNT = 4;
const uint8_t RIPPLE_COUNT = 3;

// Display update timing
uint32_t lastDisplayUpdate = 0;
const uint16_t DISPLAY_UPDATE_INTERVAL = 100;  // 100ms refresh rate

// Animation structures
struct Bubble {
  int16_t x;
  int16_t y;
  int16_t startY;
  uint8_t speed;
  bool active;
};
struct Ripple {
  int16_t y;
  uint8_t offset;
  uint8_t amplitude;
};

// Animation variables
Bubble bubbles[BUBBLE_COUNT];
Ripple ripples[RIPPLE_COUNT];
uint8_t waveOffsets[WAVE_LAYERS];
uint32_t lastBubbleTime = 0;

// Controller state variables
struct ControllerState {
  uint8_t waterLevel = 75;       // Current water level percentage
  bool pumpStatus = true;        // Pump ON/OFF state
  uint16_t dailyRunCount = 0;    // Number of times pump ran today
  uint32_t dailyRunTime = 0;     // Total runtime in seconds
  float powerConsumption = 0.0;  // Power consumption in kWh
  uint16_t timerSeconds = 0;     // Timer countdown in seconds
  bool childLock = false;        // Child lock status
  bool bypassEnabled = false;    // Level bypass status
  bool rfConnected = false;      // 433MHz connection status
  uint32_t lastHeartbeat = 0;    // Last heartbeat time
} state;

// void Display::setupWaterAnimation() {
//   for (int i = 0; i < BUBBLE_COUNT; i++) {
//     bubbles[i].active = false;
//   }

//   // for (int i = 0; i < RIPPLE_COUNT; i++) {
//   //   ripples[i].y = 5 + (i * 25);
//   //   ripples[i].offset = random(0, 255);
//   //   ripples[i].amplitude = 2 + random(2);
//   // }
// }

// void Display::updateWaterAnimation() {
//   uint32_t currentMillis = millis();

//   // Update wave layers
//   for (int i = 0; i < WAVE_LAYERS; i++) {
//     waveOffsets[i] = (waveOffsets[i] + 1 + i) % 255;
//   }

//   // Create new bubbles when pump is running
//   if (state.pumpStatus && currentMillis - lastBubbleTime > 2000) {
//     lastBubbleTime = currentMillis;
//     for (int i = 0; i < BUBBLE_COUNT; i++) {
//       if (!bubbles[i].active) {
//         bubbles[i].x = 15 + random(80);
//         bubbles[i].startY = 165 - random(20);
//         bubbles[i].y = bubbles[i].startY;
//         bubbles[i].speed = 1 + random(2);
//         bubbles[i].active = true;
//         break;
//       }
//     }
//   }
// }

// void Display::drawWaterLevel(int baseY, int levelHeight) {
//   // tft.setCursor(0, 0);
//   // tft.println("hello world");
//   // Tank outline
//   // tft.drawRect(5, 5, 60, 122, ST77XX_WHITE);

//   if (state.pumpStatus) {
//     // Animated water when pump is running
//     for (int x = 0; x < 60; x++) {
//       int offset = sin((x + waveOffsets[0]) * PI / 32) * 2;
//       int waterY = baseY + offset;
//       tft.drawFastVLine(5 + x, waterY, levelHeight - offset, ST77XX_BLUE);
//     }

//       // Ripple effects
//       // for (int i = 0; i < RIPPLE_COUNT; i++) {
//       //   if (ripples[i].y < baseY + levelHeight) {
//       //     for (int x = 0; x < 40; x += 2) {
//       //       int rippleOffset = sin((x + ripples[i].offset) * PI / 16) * ripples[i].amplitude;
//       //       tft.drawPixel(16 + x, ripples[i].y + rippleOffset, 0x867D);  // Light blue
//       //     }
//       //     ripples[i].offset = (ripples[i].offset + 1) % 255;
//       //   }
//       // }

//     //   // Draw bubbles
//     //   for (int i = 0; i < BUBBLE_COUNT; i++) {
//     //     if (bubbles[i].active) {
//     //       bubbles[i].y -= bubbles[i].speed;
//     //       if (bubbles[i].y > baseY) {
//     //         tft.drawPixel(bubbles[i].x, bubbles[i].y, ST77XX_WHITE);
//     //       } else {
//     //         bubbles[i].active = false;
//     //       }
//     //     }
//     //   }
//   } else {
//     // Static water level when pump is off
//     tft.fillRect(5, 60, 60, levelHeight, ST77XX_BLUE);
//   }

//   // tft.fillRect(5, 60, 60, levelHeight, ST77XX_BLUE);

//   // Water level text
//   // tft.setTextSize(1);
//   // tft.setTextColor(ST77XX_WHITE);
//   // tft.setCursor(22, 10);
//   // tft.print("LEVEL");
//   // tft.setTextSize(2);
//   // tft.setCursor(22, 60);
//   // tft.print(String(state.waterLevel) + "%");
// }

// void Display::drawPumpStatus() {
//   // Pump status indicator
//   uint16_t color = state.pumpStatus ? ST77XX_GREEN : ST77XX_RED;
//   tft.fillCircle(160, 40, 20, color);
//   tft.setTextSize(1);
//   tft.setTextColor(state.pumpStatus ? ST77XX_BLACK : ST77XX_WHITE);
//   tft.setCursor(145, 35);
//   tft.print(state.pumpStatus ? "ON" : "OFF");
// }

// void Display::drawRuntime() {
//   tft.setTextSize(1);
//   tft.setTextColor(ST77XX_WHITE);

//   // Daily statistics
//   tft.setCursor(230, 25);
//   tft.print("Today:");
//   tft.setCursor(230, 45);
//   tft.print(String(state.dailyRunCount));
//   tft.print(" times");

//   // Convert seconds to minutes for display
//   uint16_t minutes = state.dailyRunTime / 60;
//   tft.setCursor(230, 65);
//   tft.print(String(minutes));
//   tft.print(" mins");
// }

// void Display::drawPowerConsumption() {
//   tft.setTextSize(1);
//   tft.setTextColor(ST77XX_YELLOW);
//   tft.setCursor(140, 95);
//   tft.print("Power: ");
//   tft.print(state.powerConsumption, 1);
//   tft.print(" kWh");
// }

// void Display::drawTimer() {
//   if (state.timerSeconds > 0) {
//     tft.drawRect(120, 120, 190, 50, ST77XX_WHITE);
//     tft.setTextSize(2);
//     tft.setTextColor(ST77XX_WHITE);

//     // Format timer as MM:SS
//     uint16_t minutes = state.timerSeconds / 60;
//     uint16_t seconds = state.timerSeconds % 60;
//     char timerText[6];
//     sprintf(timerText, "%02d:%02d", minutes, seconds);

//     // Center timer text
//     tft.setCursor(170, 135);
//     tft.print(timerText);

//     tft.setTextSize(1);
//     tft.setCursor(190, 165);
//     tft.print("TIMER");
//   }
// }

// void Display::drawStatusIndicators() {
//   // Child Lock Status
//   tft.drawRoundRect(120, 180, 90, 30, 4, ST77XX_WHITE);
//   tft.setTextSize(1);
//   tft.setTextColor(ST77XX_WHITE);
//   tft.setCursor(135, 192);
//   tft.print("LOCK");
//   tft.fillCircle(185, 195, 8, state.childLock ? ST77XX_RED : ST77XX_DARKGREY);

//   // Level Bypass Status
//   tft.drawRoundRect(220, 180, 90, 30, 4, ST77XX_WHITE);
//   tft.setCursor(230, 192);
//   tft.print("BYPASS");
//   tft.fillCircle(290, 195, 8, state.bypassEnabled ? ST77XX_GREEN : ST77XX_DARKGREY);
// }

// void Display::drawRFStatus() {
//   tft.setTextSize(1);
//   tft.setTextColor(ST77XX_WHITE);
//   tft.setCursor(10, 220);
//   tft.print("433MHz:");

//   // Heartbeat indicator
//   uint32_t currentMillis = millis();
//   if (currentMillis - state.lastHeartbeat < 1000) {  // Flash every second
//     tft.fillCircle(80, 225, 6, ST77XX_GREEN);
//   } else {
//     tft.fillCircle(80, 225, 6, ST77XX_DARKGREY);
//   }
// }


// void Display::updateControllerState(uint8_t level, bool pump, uint16_t runCount,
//                                     uint32_t runtime, float power, uint16_t timer,
//                                     bool lock, bool bypass, bool rf) {
//   state.waterLevel = level;
//   state.pumpStatus = pump;
//   state.dailyRunCount = runCount;
//   state.dailyRunTime = runtime;
//   state.powerConsumption = power;
//   state.timerSeconds = timer;
//   state.childLock = lock;
//   state.bypassEnabled = bypass;
//   state.rfConnected = rf;
//   if (rf) state.lastHeartbeat = millis();
// }

// void Display::updateDisplay() {
//   uint32_t currentMillis = millis();
//   if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
//     lastDisplayUpdate = currentMillis;

//     // Clear display
//     // tft.fillScreen(ST77XX_BLACK);

//     // Calculate water level height
//     int levelHeight = map(state.waterLevel, 0, 100, 0, 115);
//     int baseY = 165 - levelHeight;

//     // Draw all display elements
//     drawWaterLevel(baseY, levelHeight);
//     // drawPumpStatus();
//     // drawRuntime();
//     // drawPowerConsumption();
//     // drawTimer();
//     // drawStatusIndicators();
//     // drawRFStatus();

//     if (state.pumpStatus) {
//       updateWaterAnimation();
//     }
//   }
// }

void Display::initDisplay() {
  // Initialize display
  tft.initR(INITR_BLACKTAB);
  // Make the display landscape.
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  // setWaterLevel();

  // setupWaterAnimation();
}

void Display::setWaterLevel(uint16_t waterLevel){
  int levelHeight = map(state.waterLevel, 0, 100, 0, 115);
  int baseY = 165 - levelHeight;
  String title = "LEVEL";
  tft.drawRect(5, 5, 60, 122, ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  Coordinate titleXY = getCenterPosition(title, 5, 5, 60, 122, 1);
  tft.setCursor(titleXY.x, 10);
  tft.println(title);

  int lineHeight = 8 * 2;
  //            x, y, w, h, color
  String text = String(waterLevel) + "%";
  Coordinate textXY = getCenterPosition(text, 5, 5, 60, 122, 2);
  tft.fillRect(6, textXY.y, 57, lineHeight, ST77XX_BLACK);  // Text color, Background color;
  tft.setCursor(textXY.x, textXY.y);
  tft.setTextSize(2);
  tft.println(String(waterLevel) + "%");
}

Display::Coordinate Display::getCenterPosition(String text, int boxX, int boxY, int boxW, int boxH, int textSize){
  int outputX, outputY, textW, textH;
  tft.setTextSize(textSize);
  tft.getTextBounds(text.c_str(), boxX, boxY, &outputX, &outputY, &textW, &textH);
  Coordinate centerCoordinate;
  centerCoordinate.x = boxX + (boxW - textW) / 2;
  centerCoordinate.y = boxY + (boxH - textH) / 2;
  Serial.println(textW);

  return centerCoordinate;

}
