#include "control_box.h"

#define SENSITIVITY 409.0f

// ACS712 settings (20A model)
const float CURRENT_SENSITIVITY = 0.100;  // 100mV/A for 20A model
const float CURRENT_OFFSET = 2.5;

// Timing and averaging settings
const unsigned long SAMPLING_INTERVAL = 1000;  // Sample every 100ms when load is ON
const unsigned long ONE_DAY_MS = 86400000;     // 24 hours in milliseconds
const int READINGS_PER_AVERAGE = 10;           // Number of readings to average

ControlBox::ControlBox(
  int relaySwitchPin = 4,
  int leftArrowSwitchPin = 7,
  int powerSwitchPin = 6,
  int rightArrowSwitchPin = 5,
  int voltageSensorPin = A0,
  int currentSensorPin = A1)
  : voltageSensor(voltageSensorPin, DEFAULT_FREQUENCY) {
  this->powerSwitchPin = powerSwitchPin;
  this->relaySwitchPin = relaySwitchPin;
  this->leftArrowSwitchPin = leftArrowSwitchPin;
  this->rightArrowSwitchPin = rightArrowSwitchPin;
  this->voltageSensorPin = voltageSensorPin;
  this->currentSensorPin = currentSensorPin;
}


void ControlBox::setup() {
  display.setup();
  // Initial Heart beat should be false and this method must be called at setup.
  setHeartBeat(false);
  pinMode(relaySwitchPin, OUTPUT);
  digitalWrite(relaySwitchPin, HIGH);
  pinMode(powerSwitchPin, INPUT_PULLUP);
  pinMode(leftArrowSwitchPin, INPUT_PULLUP);
  pinMode(rightArrowSwitchPin, INPUT_PULLUP);
  // Configure ADC reference
  analogReference(DEFAULT);
  voltageSensor.setSensitivity(SENSITIVITY);
}

void ControlBox::loop() {
  checkHeartBeat();
  onClickPowerSwitch();
  onClicLeftArrowSwitch();
  onClicRightArrowSwitch();
  autoPowerOnOff();
  onTimerActivate();
  addTimer();
  minusTimer();
  onTimerSwitchOff();
  measureWattPower();
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
  // Low for realy switch on. If pumpStatus is HIGH/ true then relay pin should be low.
  digitalWrite(relaySwitchPin, !state.pumpStatus);
  display.drawPumpStatus(state.pumpStatus);
  if (state.pumpStatus) {
    state.pumpStartTime = millis();
    state.pumpRunCnt++;
    display.drawRuncount(state.pumpRunCnt);
  } else if (state.pumpStartTime > 0) {
    measureWattPower(true);
    state.pumpStartTime = 0;
  }
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
    cancelTimer();
  }
}

void ControlBox::onTimerActivate() {
  if (state.powerSwitchClickCnt < 2) return;
  if (state.powerSwitchClickCnt >= 4) {
    state.powerSwitchClickCnt = 4;
  } else if (state.powerSwitchClickCnt < 2) {
    state.timerSettingStartTime = millis();
    state.timerDisplayBlinkTime = millis();
  }
  if (millis() - state.timerDisplayBlinkTime >= 150) {
    if (state.hideTimer) {
      display.clearTimerDisplay();
    } else {
      TimeConversionResult convertedTimer = convertMiliToMinSec();
      display.drawTimer(convertedTimer.minutes, convertedTimer.seconds);
    }
    state.timerDisplayBlinkTime = millis();
    state.hideTimer = !state.hideTimer;
  }
}

void ControlBox::addTimer() {
  if (state.childLock || state.powerSwitchClickCnt != 2) return;
  unsigned long spendTime = millis() - state.timerSettingStartTime;
  bool switchState = digitalRead(rightArrowSwitchPin);
  bool add = false;
  if (switchState == LOW && state.rightArrowSwitchState == HIGH) {
    add = true;
  } else if (switchState == LOW && state.rightArrowSwitchState == LOW && spendTime >= 400) {
    add = true;
  }
  if (add == true && state.timer < 5940000) {
    state.timer += 60000;
    state.timerSettingStartTime = millis();
  }
}

void ControlBox::minusTimer() {
  if (state.childLock || state.powerSwitchClickCnt != 2) return;
  unsigned long spendTime = millis() - state.timerSettingStartTime;
  bool switchState = digitalRead(leftArrowSwitchPin);
  bool minus = false;
  if (switchState == LOW && state.leftArrowSwitchState == HIGH) {
    minus = true;
  } else if (switchState == LOW && state.leftArrowSwitchState == LOW && spendTime >= 400) {
    minus = true;
  }
  if (minus == true && state.timer >= 60000) {
    state.timer -= 60000;
    state.timerSettingStartTime = millis();
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
  if(!state.heartBeat && !state.bypass){
    cancelTimer();
  }

  state.hideTimer = false;
  resetPowerSwitchState();
  if (state.timer > 0) {
    changePumpStatus(true);
    state.timerStartTime = state.pumpStartTime;
    display.clearTimerDisplay();
    TimeConversionResult convertedTimer = convertMiliToMinSec();
    display.drawTimer(convertedTimer.minutes, convertedTimer.seconds);
  } else {
    cancelTimer();
  }
}

void ControlBox::onTimerSwitchOff() {
  if (state.timer == 0 || state.powerSwitchClickCnt > 1) return;
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - state.timerStartTime;

  if (elapsedTime >= 1000 && state.timer >= 1000) {
    state.timer -= elapsedTime;
    state.timerStartTime = currentTime;
    TimeConversionResult convertedTimer = convertMiliToMinSec();
    display.drawTimer(convertedTimer.minutes, convertedTimer.seconds);
  }
  else if (state.timer <= 1000) {
    state.timer -= elapsedTime;
    state.timerStartTime = currentTime;
  }
  if (state.timer <= 0){
     cancelTimer();
  }
}

ControlBox::TimeConversionResult ControlBox::convertMiliToMinSec() {
  TimeConversionResult result;
  unsigned long totalSeconds = state.timer / 1000;
  result.minutes = totalSeconds / 60;
  result.seconds = totalSeconds % 60;

  return result;
}


float ControlBox::measureCurrent() {
  float sum = 0;
  for (int i = 0; i < 10; i++) {
    float raw = analogRead(currentSensorPin);
    float voltage = (raw * 5.0) / 1023.0;
    sum += abs((voltage - CURRENT_OFFSET) / CURRENT_SENSITIVITY);
  }
  return sum / 10.0;
}

float ControlBox::measureVoltage() {
  return voltageSensor.getRmsVoltage(10);
}

void ControlBox::measureWattPower(bool force = false) {
  unsigned long currentTime = millis();
  if ( force || (state.pumpStartTime > 0 && currentTime - state.pumpStartTime  >= SAMPLING_INTERVAL)) {
    state.pumpTotalRunTime += (currentTime - state.pumpStartTime);
    display.drawRunTime(state.pumpTotalRunTime / 60000.0);
  }
  if (!force && (!state.pumpStatus || currentTime - state.pumpStartTime < SAMPLING_INTERVAL)) return;

  float current = measureCurrent();
  float voltage = measureVoltage();

  // Calculate elapsed time.
  float elapsedTimeHours = (currentTime - state.pumpStartTime) / 3600000.0;

  // Compute energy in kWh
  // float powerkW = (current * voltage) / 1000.0;
  float powerW = (current * voltage);
  state.powerConsumption += powerW * elapsedTimeHours;
  // Display total kWh
  display.drawPowerConsumption(state.powerConsumption);

  // Reset time tracker.
  state.pumpStartTime = currentTime;
}
