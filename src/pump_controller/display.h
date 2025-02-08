#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeSerif18pt7b.h>

class Display {
public:
  struct Coordinate {
    int x, y;
  };
  // void updateWaterAnimation();
  // void drawWaterLevel(int baseY, int levelHeight);
  // void drawPumpStatus();
  // void drawRuntime();
  // void drawPowerConsumption();
  // void drawTimer();
  // void drawStatusIndicators();
  // void drawRFStatus();
  // void updateControllerState(uint8_t level, bool pump, uint16_t runCount,
  //                            uint32_t runtime, float power, uint16_t timer,
  //                            bool lock, bool bypass, bool rf);
  // void updateDisplay();
  void initDisplay();
  void setWaterLevel(uint16_t distance);
  Coordinate getCenterPosition(String text, int boxX, int boxY, int boxW, int boxH, int textSize);
  void togglePumpStatus();
  // void setupWaterAnimation();
private:
  void drawWaterLevel();
  void drawWaterTank();
  uint8_t getWaterLevelY();
  uint8_t getWaterLevel(uint16_t distance);
  uint8_t getWaterLevelHight();
  void drawPumpStatus();
//   Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
//   struct ControllerState {
//     uint8_t waterLevel = 75;
//     bool pumpStatus = false;
//     uint16_t dailyRunCount = 0;
//     uint32_t dailyRunTime = 0;
//     float powerConsumption = 0.0;
//     uint16_t timerSeconds = 0;
//     bool childLock = false;
//     bool bypassEnabled = false;
//     bool rfConnected = false;
//     uint32_t lastHeartbeat = 0;
//   } state;
};

#endif
