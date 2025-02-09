#include <SPI.h>  // Not actually used but needed to compile
#include <string.h>
#include "water_tank_data_receiver.h"
#include "control_box.h"

ControlBox controlBox;
WaterTankDataController waterTankDataController(controlBox);


void setup() {
  Serial.begin(9600);  // Starts the serial communication
  controlBox.init();
  waterTankDataController.init();
}

void loop() {
  // Receive any new data from water level measurement controller.
  // receiveWaterDistance();
  // delay(1000);
  // display.();
  // delay(10);
  waterTankDataController.receiveWaterDistance();
  controlBox.checkHeartBeat();
  // controlBox.display.setWaterLevel(1000);
  // display.drawTodayHistory(false, 100, 1500, 1500);
  // display.drawPumpStatus();
  // delay(3000);
  // display.setWaterLevel(800);
  // delay(3000);
  // display.setWaterLevel(300);
  // delay(3000);
  // display.setWaterLevel(0);
  // display.drawPumpStatus(true);
  // display.drawTodayHistory(true);
  // display.drawBypass(true);
  // display.drawChildLock(true);
  // display.drawWaterTankHearBeat(true);
  // display.drawTimer(49, 80);
  delay(3000);
}
