#include "control_box.h"

ControlBox::ControlBox(int relaySwitchPin = 4, int leftArrowSwitchPin = 7, int powerSwitchPin = 6, int rightArrowSwitchPin = 5) {
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
  onTimerActivate();
  upTimer();
  minusTimer();
}

void ControlBox::onClickPowerSwitch() {
  if (state.childLock) return;

  bool switchState = digitalRead(powerSwitchPin);
  unsigned long timeElapsed = millis() - state.powerSwitchStartTime;
  if (switchState == LOW && state.powerSwitchState == HIGH) {
    state.powerSwitchStartTime = millis();
    state.powerSwitchClickCnt++;
  } else if (switchState == LOW
             && timeElapsed >= 500
             && state.powerSwitchStartTime > 0
             && (state.bypass || state.heartBeat)
             && state.powerSwitchClickCnt < 2) {
    togglePower();
    resetPowerSwitchState();
  } else if (state.powerSwitchState == HIGH && state.powerSwitchClickCnt == 1 && timeElapsed > 500) {
    resetPowerSwitchState();
  } else if (state.powerSwitchClickCnt == 3 && timeElapsed >= 300) {
    startTimer();
  } else if (state.powerSwitchClickCnt == 4) {
    cancelTimer();
  }

  state.powerSwitchState = switchState;
}

void ControlBox::onClicLeftArrowSwitch() {
  bool switchState = digitalRead(leftArrowSwitchPin);
  if (switchState == LOW && state.leftArrowSwitchState == HIGH && state.powerSwitchClickCnt < 2) {
    state.childLockSwitchStartTime = millis();
  } else if (switchState == LOW && (millis() - state.childLockSwitchStartTime) >= 1500 && state.childLockSwitchStartTime > 0 && state.powerSwitchClickCnt <= 2) {
    toggleChildLock();
    state.childLockSwitchStartTime = 0;
  }

  state.leftArrowSwitchState = switchState;
}

void ControlBox::onClicRightArrowSwitch() {
  if (state.childLock) return;
  bool switchState = digitalRead(rightArrowSwitchPin);
  if (switchState == LOW && state.rightArrowSwitchState == HIGH && state.powerSwitchClickCnt < 2) {
    state.bypassSwitchStartTime = millis();
  } else if (switchState == LOW && (millis() - state.bypassSwitchStartTime) >= 1000 && state.bypassSwitchStartTime > 0 && state.powerSwitchClickCnt < 2) {
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
  if (!state.pumpStatus) {
    cancelTimer();
  }
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
    cancelTimer();
  }
  if (getWaterLevel() > 90 && state.heartBeat && state.pumpStatus) {
    changePumpStatus(false);
    cancelTimer();
  }
  if (!state.heartBeat && (millis() - state.lastUpdatedHeartBeat) >= 30000 && state.pumpStatus) {
    changePumpStatus(false);
  }
}

void ControlBox::onTimerActivate() {
  if (state.powerSwitchClickCnt >= 4) {
    state.powerSwitchClickCnt = 4;
  }
  if (state.powerSwitchClickCnt < 2) {
    state.timerSettingStartTime = millis();
    return;
  }
  if (millis() - state.timerSettingStartTime >= 400) {
    if (state.hideTimer) {
      display.clearTimerDisplay();
    } else {
      display.drawTimer(state.timer);
    }
    state.timerSettingStartTime = millis();
    state.hideTimer = !state.hideTimer;
  }
}

void ControlBox::upTimer() {
  if (state.childLock) return;
  uint8_t tempTimer = state.timer;
  bool switchState = digitalRead(rightArrowSwitchPin);
  if (switchState == LOW && state.leftArrowSwitchState == HIGH && state.powerSwitchClickCnt == 2) {
    tempTimer--;
  } else if (switchState == LOW && state.leftArrowSwitchState == LOW && state.powerSwitchClickCnt == 2 && (millis() - state.timerSettingStartTime) >= 300) {
    state.timerSettingStartTime = 0;
    tempTimer--;
  }

  if (tempTimer <= 0) {
    state.timer = 0;
  } else {
    state.timer = tempTimer;
  }
}

void ControlBox::minusTimer() {
  if (state.childLock) return;
  uint8_t tempTimer = state.timer;
  bool switchState = digitalRead(leftArrowSwitchPin);
  if (switchState == LOW && state.leftArrowSwitchState == HIGH && state.powerSwitchClickCnt == 2) {
    tempTimer++;
  } else if (switchState == LOW && state.leftArrowSwitchState == LOW && state.powerSwitchClickCnt == 2 && (millis() - state.timerSettingStartTime) >= 300) {
    state.timerSettingStartTime = 0;
    tempTimer++;
  }

  if (tempTimer >= 99) {
    state.timer = 99;
  } else {
    state.timer = tempTimer;
  }
}

void ControlBox::resetPowerSwitchState(uint8_t clickCnt = 0) {
  state.powerSwitchStartTime = 0;
  state.powerSwitchClickCnt = clickCnt;
}

void ControlBox::cancelTimer() {
  state.timer = 0;
  display.clearTimerDisplay();
  display.drawTimer(state.timer);
  state.hideTimer = false;
  resetPowerSwitchState();
  changePumpStatus();
}

void ControlBox::startTimer() {
  display.clearTimerDisplay();
  display.drawTimer(state.timer);
  state.hideTimer = false;
  resetPowerSwitchState();
  if (state.timer > 0) {
    changePumpStatus(true);
  }
}
