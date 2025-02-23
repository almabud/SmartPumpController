#include "water_tank_data_receiver.h"
#include "control_box.h"

ControlBox controlBox;
WaterTankDataController waterTankDataController(controlBox);


void setup() {
  Serial.begin(9600);  // Starts the serial communication
  controlBox.setup();
  waterTankDataController.setup();
}

void loop() {

  waterTankDataController.loop();
  controlBox.loop();
}
