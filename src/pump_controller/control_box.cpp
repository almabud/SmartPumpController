#include "control_box.h"

ControlBox::ControlBox(int powerSwitchPin = 6, int relaySwitchPin = 8) {
  this->powerSwitchPin = powerSwitchPin;
  this->relaySwitchPin = relaySwitchPin;
}

void ControlBox::setup() {
  display.setup();
  // Initial Heart beat should be false and this method must be called at setup.
  setHeartBeat(false);
  pinMode(powerSwitchPin, INPUT_PULLUP);
}

void ControlBox::loop() {
  checkHeartBeat();
  onClickPowerSwitch();
  autoPowerOnOff();
}

void ControlBox::onClickPowerSwitch() {
  bool powerSwitchState = digitalRead(powerSwitchPin);
  if (powerSwitchState == LOW && state.powerSwitchState == HIGH) {
    state.powerSwitchStartTime = millis();
  }
  if (powerSwitchState == LOW && (millis() - state.powerSwitchStartTime) >= 500 && state.powerSwitchStartTime > 0 && (state.bypass || state.heartBeat)) {
    togglePower();
    state.powerSwitchStartTime = 0;
  }

  state.powerSwitchState = powerSwitchState;
}

void ControlBox::changePumpStatus(bool status = false) {
  state.pumpStatus = status;
  digitalWrite(relaySwitchPin, state.pumpStatus);
  display.drawPumpStatus(state.pumpStatus);
}

void ControlBox::togglePower() {
  changePumpStatus(!state.pumpStatus);
}

ControlBox::ControlBoxState ControlBox::getState() const {
  return state;
}

void ControlBox::checkHeartBeat() {
  if (millis() - state.lastUpdatedHeartBeat >= 3000 && state.heartBeat) {
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

void ControlBox::autoPowerOnOff() {
  if (!state.bypass && getWaterLevel() <= 5 && state.heartBeat && !state.pumpStatus) {
    changePumpStatus(true);
  }
  if (!state.bypass && getWaterLevel() > 90 && state.heartBeat && state.pumpStatus) {
    changePumpStatus(false);
  }
  if (!state.bypass && !state.heartBeat && (millis() - state.lastUpdatedHeartBeat) >= 30000 && state.pumpStatus) {
    changePumpStatus(false);
  }
}