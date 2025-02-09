#include "control_box.h"

void ControlBox::setup() {
  display.setup();
  pinMode(powerSwitchPin, INPUT_PULLUP);
}

void ControlBox::loop() {
  checkHeartBeat();
  powerSwitch();
}

void ControlBox::powerSwitch() {
  bool powerSwitchState = digitalRead(powerSwitchPin);
  if (powerSwitchState == LOW && state.powerSwitchState == HIGH) {
    state.powerSwitchStartTime = millis();
  }
  if (powerSwitchState == LOW && (millis() - state.powerSwitchStartTime) >= 500 && state.powerSwitchStartTime > 0) {
    togglePower();
    state.powerSwitchStartTime = 0;
  }
  
  state.powerSwitchState = powerSwitchState;
}

void ControlBox::togglePower() {
  state.pumpSwitchStatus = !state.pumpSwitchStatus;
  digitalWrite(powerSwitchPin, state.pumpSwitchStatus);
  display.drawPumpStatus(state.pumpSwitchStatus);
}

ControlBox::ControlBoxState ControlBox::getState() const {
  return state;
}

void ControlBox::checkHeartBeat() {
  if (millis() - state.lastUpdatedHeartBeat >= 3000) {
    setHeartBeat(false);
  }
}

uint8_t ControlBox::getWaterLevel(uint16_t distance = UINT16_MAX) {
  if (distance == UINT16_MAX) {
    return map(state.waterDistance, 0, 130, 0, 100);
  }

  return map(distance, 0, 130, 0, 100);
}

void ControlBox::setWaterDistance(uint16_t distance) {
  uint8_t waterLevel = getWaterLevel(distance);
  uint8_t changeWaterLevel = abs(distance - state.waterDistance);
  if (changeWaterLevel <= 3 && state.waterDistance != UINT16_MAX) {
    return;
  }
  state.waterDistance = distance;
  display.setWaterLevel(waterLevel);
}

void ControlBox::setHeartBeat(bool heartBeat) {
  state.heartBeat = heartBeat;
  state.lastUpdatedHeartBeat = millis();
  display.drawWaterTankHearBeat(heartBeat);
}