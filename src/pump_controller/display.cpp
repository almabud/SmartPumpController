#include "display.h"

#define TFT_CS A5
#define TFT_RST A4
#define TFT_DC A3
#define ST77XX_DARKGREY 0x7BEF  // 123, 123, 123

// Initialize display module.
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// water tank constants.
const uint8_t WATER_TANK_START_X = 1;
const uint8_t WATER_TANK_START_Y = 1;
const uint8_t WATER_TANK_WIDTH = 53;
const uint8_t WATER_TANK_HIGHT = 127;
uint8_t waterLevelState = 255;


void Display::setup() {
  // Initialize display
  tft.initR(INITR_BLACKTAB);
  // Make the display landscape.
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  drawWaterTank();
  drawPumpStatus();
  drawTodayHistory();
  drawChildLock();
  drawBypass();
  drawTimer();
  drawWaterTankHearBeat();
}

void Display::setWaterLevel(uint8_t waterLevel) {
  if (waterLevelState == waterLevel && waterLevelState <= 100) {
    return;
  }

  waterLevelState = waterLevel;
  drawWaterLevel();
  int baseY = 165 - getWaterLevelHight();

  int lineHeight = 8 * 2;
  String text = String(waterLevelState) + "%";
  Coordinate textXY = getCenterPosition(text, WATER_TANK_START_X, WATER_TANK_START_Y, WATER_TANK_WIDTH, WATER_TANK_HIGHT, 2);
  // Remove the existing write on display.
  if (textXY.y >= getWaterLevelY()) {
    tft.fillRect(WATER_TANK_START_X + 1, textXY.y, WATER_TANK_WIDTH - 3, lineHeight, ST77XX_BLUE);
  } else {
    tft.fillRect(WATER_TANK_START_X + 1, textXY.y, WATER_TANK_WIDTH - 3, lineHeight, ST77XX_BLACK);
  }
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(textXY.x, textXY.y);
  tft.setTextSize(2);
  tft.println(String(waterLevelState) + "%");
}

Display::Coordinate Display::getCenterPosition(String text, int boxX, int boxY, int boxW, int boxH, int textSize) {
  int outputX, outputY, textW, textH;
  tft.setTextSize(textSize);
  tft.getTextBounds(text.c_str(), boxX, boxY, &outputX, &outputY, &textW, &textH);
  Coordinate centerCoordinate;
  centerCoordinate.x = boxX + (boxW - textW) / 2;
  centerCoordinate.y = boxY + (boxH - textH) / 2;

  return centerCoordinate;
}

void Display::drawPumpStatus(bool pumpStatus = false) {
  int8_t radius = 15;
  int8_t switchX = WATER_TANK_WIDTH + WATER_TANK_START_X + radius + 3;
  int8_t switchY = WATER_TANK_START_Y + radius;
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  if (pumpStatus) {
    tft.setTextColor(ST77XX_BLACK);
    tft.setCursor(switchX - 6, switchY - 3);
    //tft.fillCircle(x, y, radius, color)
    tft.fillCircle(switchX, switchY, radius, ST77XX_GREEN);
    tft.print("ON");
  } else {
    tft.setCursor(switchX - 7, switchY - 3);
    tft.fillCircle(switchX, switchY, radius, ST77XX_RED);
    tft.print("OFF");
  }
}

void Display::drawChildLock(bool status = false) {
  Coordinate centerPostion = getCenterPosition("CHILD LOCK", WATER_TANK_START_X + WATER_TANK_WIDTH + 38, WATER_TANK_START_Y, 120 - WATER_TANK_WIDTH, 14, 1);
  tft.setTextColor(ST77XX_WHITE);
  // Draw boundary.
  tft.drawRect(WATER_TANK_START_X + WATER_TANK_WIDTH + 38, WATER_TANK_START_Y, 120 - WATER_TANK_WIDTH, 14, ST77XX_WHITE);
  if (status) {
    tft.setTextColor(ST77XX_BLACK);
    tft.fillRect(WATER_TANK_START_X + WATER_TANK_WIDTH + 39, WATER_TANK_START_Y + 1, 118 - WATER_TANK_WIDTH, 12, ST77XX_GREEN);
  } else {
    tft.setTextColor(ST77XX_WHITE);
    tft.fillRect(WATER_TANK_START_X + WATER_TANK_WIDTH + 39, WATER_TANK_START_Y + 1, 118 - WATER_TANK_WIDTH, 12, ST77XX_RED);
  }
  tft.setCursor(centerPostion.x, centerPostion.y);
  tft.print("CHILD LOCK");
}

void Display::drawBypass(bool status = false) {
  Coordinate centerPostion = getCenterPosition("BYPASS", WATER_TANK_START_X + WATER_TANK_WIDTH + 38, WATER_TANK_START_Y + 17, 120 - WATER_TANK_WIDTH, 14, 1);
  // Draw boundary.
  tft.drawRect(WATER_TANK_START_X + WATER_TANK_WIDTH + 38, WATER_TANK_START_Y + 17, 120 - WATER_TANK_WIDTH, 14, ST77XX_WHITE);
  if (status) {
    tft.setTextColor(ST77XX_BLACK);
    tft.fillRect(WATER_TANK_START_X + WATER_TANK_WIDTH + 39, WATER_TANK_START_Y + 18, 118 - WATER_TANK_WIDTH, 12, ST77XX_GREEN);
  } else {
    tft.setTextColor(ST77XX_WHITE);
    tft.fillRect(WATER_TANK_START_X + WATER_TANK_WIDTH + 39, WATER_TANK_START_Y + 18, 118 - WATER_TANK_WIDTH, 12, ST77XX_RED);
  }
  tft.setCursor(centerPostion.x, centerPostion.y);
  tft.print("BYPASS");
}

void Display::drawTodayHistory(bool today = false, int8_t runCount = -1, int16_t runTime = -1, int16_t powerConsumption = -1) {
  String title = "Last 24h";
  uint8_t boxX = WATER_TANK_START_X + WATER_TANK_WIDTH + 2;
  uint8_t boxY = 35;
  uint8_t boxW = 155 - WATER_TANK_WIDTH;
  uint8_t boxH = 50;
  if (today) {
    title = "Today";
  }
  tft.setTextColor(ST77XX_WHITE);
  // String text, int boxX, int boxY, int boxW, int boxH, int textSize
  Coordinate centerPosition = getCenterPosition(title, boxX, boxY, boxW, boxH, 1);
  // Draw boundary.
  tft.drawRect(boxX, boxY, boxW, boxH, ST77XX_WHITE);
  // Remove Existing draw.
  tft.fillRect(boxX + 1, boxY + 1, boxW - 3, boxH - 2, ST77XX_BLACK);
  tft.fillRect(boxX + 1, boxY + 1, boxW - 3, 12, ST77XX_DARKGREY);
  tft.setTextSize(1);
  tft.setCursor(centerPosition.x, boxY + 2);
  tft.println(title);

  if (runCount > 0 && runTime > 0) {
    tft.setCursor(boxX + 2, boxY + 15);
    tft.println("Count:" + String(runTime));
    tft.setCursor(boxX + 2, boxY + 26);
    tft.println("Time:" + String(runTime) + " min");
  }

  if (powerConsumption >= 0) {
    tft.setCursor(boxX + 2, boxY + 38);
    tft.println("Power:" + String(powerConsumption) + " KWH");
  }
}

void Display::clearTimerDisplay() {
  uint8_t boxX = WATER_TANK_START_X + WATER_TANK_WIDTH + 2;
  uint8_t boxY = 88;
  uint8_t boxW = 155 - WATER_TANK_WIDTH;
  tft.drawRect(boxX, boxY, boxW, 24, ST77XX_WHITE);
  tft.fillRect(boxX + 1, boxY + 1, boxW - 2, 22, ST77XX_BLACK);
}

void Display::drawTimer(uint8_t min = 0, uint8_t sec = 0) {
  uint8_t boxX = WATER_TANK_START_X + WATER_TANK_WIDTH + 2;
  uint8_t boxY = 88;
  uint8_t boxW = 155 - WATER_TANK_WIDTH;
  clearTimerDisplay();
  String timer = "";

  if (min < 10) {
    timer += "0" + String(min);
  } else {
    timer += String(min);
  }

  if (sec < 10) {
    timer += ":0" + String(sec);
  } else {
    timer += ":" + String(sec);
  }

  Coordinate centerPosition = getCenterPosition(timer, boxX, boxY, boxW, 24, 2);
  tft.setCursor(centerPosition.x, centerPosition.y);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.print(timer);
}

void Display::drawWaterTankHearBeat(bool status = false) {
  uint8_t boxX = WATER_TANK_START_X + WATER_TANK_WIDTH + 2;
  uint8_t boxY = 115;
  uint8_t boxW = 155 - WATER_TANK_WIDTH;
  // Draw boundary.
  tft.drawRect(boxX, boxY, boxW, 12, ST77XX_WHITE);
  Coordinate centerPostion = getCenterPosition("HEART BEAT", boxX, boxY, boxW, 10, 1);

  if (status) {
    tft.setTextColor(ST77XX_BLACK);
    tft.fillRect(boxX + 1, boxY + 1, boxW - 2, 10, ST77XX_GREEN);
  } else {
    tft.setTextColor(ST77XX_WHITE);
    tft.fillRect(boxX + 1, boxY + 1, boxW - 2, 10, ST77XX_RED);
  }
  tft.setCursor(centerPostion.x, centerPostion.y + 1);
  tft.print("HEART BEAT");
}

// Private
void Display::drawWaterTank() {
  String title = "LEVEL";
  tft.drawRect(WATER_TANK_START_X, WATER_TANK_START_Y, WATER_TANK_WIDTH, WATER_TANK_HIGHT, ST77XX_WHITE);
  tft.fillRect(WATER_TANK_START_X + 1, WATER_TANK_START_Y + 1, WATER_TANK_WIDTH - 2, 14, ST77XX_DARKGREY);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  Coordinate titleXY = getCenterPosition(title, WATER_TANK_START_X, WATER_TANK_START_Y, WATER_TANK_WIDTH, WATER_TANK_HIGHT, 1);
  tft.setCursor(titleXY.x, WATER_TANK_START_Y + 3);
  tft.println(title);
}

void Display::drawWaterLevel(bool pumpStatus = false) {
  tft.fillRect(WATER_TANK_START_X + 1, 17, WATER_TANK_WIDTH - 2, WATER_TANK_HIGHT, ST77XX_BLACK);
  tft.fillRect(WATER_TANK_START_X + 1, getWaterLevelY(), WATER_TANK_WIDTH - 2, getWaterLevelHight(), ST77XX_BLUE);
  if (pumpStatus) {
  }
}

uint8_t Display::getWaterLevelY() {
  return WATER_TANK_HIGHT - getWaterLevelHight() + 10;
}

uint8_t Display::getWaterLevelHight() {
  uint8_t hight = map(waterLevelState, 0, 100, 0, WATER_TANK_HIGHT - (8 * 1));

  return hight;
}
