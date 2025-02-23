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
  void setup();
  void setWaterLevel(uint8_t distance);
  Coordinate getCenterPosition(String text, int boxX, int boxY, int boxW, int boxH, int textSize);
  void drawPumpStatus(bool pumpStatus = false);
  void drawChildLock(bool status = false);
  void drawBypass(bool status = false);
  void drawTodayHistoryCanvas();
  void drawTodayHistoryTitle(bool today = false);
  void drawRunTime(float runTime = 0.0);
  void drawRuncount(uint16_t runCount = 0);
  void drawPowerConsumption(float powerConsumption = 0.0);
  void drawTodayHistory(bool today = false, uint16_t runCount = 0, float runTime = 0, float powerConsumption = 0.0);
  void clearTimerDisplay();
  void drawTimer(uint8_t min = 0, uint8_t sec = 0);
  void drawWaterTankHearBeat(bool status = false);
private:
  void drawWaterLevel(bool pumpStatus = false);
  void drawWaterTank();
  uint8_t getWaterLevelY();
  uint8_t getWaterLevelHight();
};

#endif
