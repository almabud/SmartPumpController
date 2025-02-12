#include "control_box.h"

ControlBox::ControlBox(int rightArrowSwitchPin = 5, int powerSwitchPin = 6, int leftArrowSwitchPin = 7, int relaySwitchPin = 8) {
  this->powerSwitchPin = powerSwitchPin;
  this->relaySwitchPin = relaySwitchPin;
  this->leftArrowSwitchPin = leftArrowSwitchPin;  // This should be the child lock switch.
}

void ControlBox::setup() {
  display.setup();
  // Initial Heart beat should be false and this method must be called at setup.
  setHeartBeat(false);
  pinMode(powerSwitchPin, INPUT_PULLUP);
  pinMode(leftArrowSwitchPin, INPUT_PULLUP);
  pinMode(rightArrowSwitchPin, INPUT_PULLUP);
}

void ControlBox::loop() {
  checkHeartBeat();
  onClickPowerSwitch();
  onClicLeftArrowSwitch();
  onClicRightArrowSwitch();
  autoPowerOnOff();
}

void ControlBox::onClickPowerSwitch() {
  if (state.childLock) return;

  bool switchState = digitalRead(powerSwitchPin);
  if (switchState == LOW && state.powerSwitchState == HIGH) {
    state.powerSwitchStartTime = millis();
  }
  if (switchState == LOW && (millis() - state.powerSwitchStartTime) >= 500 && state.powerSwitchStartTime > 0 && (state.bypass || state.heartBeat)) {
    togglePower();
    state.powerSwitchStartTime = 0;
  }

  state.powerSwitchState = switchState;
}

void ControlBox::onClicLeftArrowSwitch() {
  bool switchState = digitalRead(leftArrowSwitchPin);
  if (switchState == LOW && state.leftArrowSwitchState == HIGH) {
    state.childLockSwitchStartTime = millis();
  }
  if (switchState == LOW && (millis() - state.childLockSwitchStartTime) >= 1500 && state.childLockSwitchStartTime > 0) {
    toggleChildLock();
    state.childLockSwitchStartTime = 0;
  }

  state.leftArrowSwitchState = switchState;
}

void ControlBox::onClicRightArrowSwitch() {
  if (state.childLock) return;
  bool switchState = digitalRead(rightArrowSwitchPin);
  if (switchState == LOW && state.rightArrowSwitchState == HIGH) {
    state.bypassSwitchStartTime = millis();
  }
  if (switchState == LOW && (millis() - state.bypassSwitchStartTime) >= 1000 && state.bypassSwitchStartTime > 0) {
    toggleBypass();
    state.bypassSwitchStartTime = 0;
  }

  state.rightArrowSwitchState = switchState;
}

void ControlBox::changePumpStatus(bool status = false) {
  state.pumpStatus = status;
  digitalWrite(relaySwitchPin, state.pumpStatus);
  display.drawPumpStatus(state.pumpStatus);
}

void ControlBox::togglePower() {
  changePumpStatus(!state.pumpStatus);
}

void ControlBox::toggleChildLock() {
  state.childLock = !state.childLock;
  display.drawChildLock(state.childLock);
}

void ControlBox::toggleBypass() {
  state.bypass = !state.bypass;
  display.drawBypass(state.bypass);
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
  if (state.bypass) return;
  if (getWaterLevel() <= 5 && state.heartBeat && !state.pumpStatus) {
    changePumpStatus(true);
  }
  if (getWaterLevel() > 90 && state.heartBeat && state.pumpStatus) {
    changePumpStatus(false);
  }
  if (!state.heartBeat && (millis() - state.lastUpdatedHeartBeat) >= 30000 && state.pumpStatus) {
    changePumpStatus(false);
  }
}