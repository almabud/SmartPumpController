#ifndef CONTROL_BOX_H
#define CONTROL_BOX_H

#include <Arduino.h>
#include <ZMPT101B.h>
#include "display.h"

class ControlBox {
public:
  struct ControlBoxState {
    uint16_t waterDistance = UINT16_MAX;
    uint16_t pumpRunCnt = 0;
    unsigned long pumpTotalRunTime = 0; // miliseconds.
    float powerConsumption = 0;
    unsigned long pumpStartTime = 0;
    // Hear Beat.
    bool heartBeat = false;
    unsigned long lastUpdatedHeartBeat = 0;
    // Power switch.
    unsigned long powerSwitchStartTime = 0;
    bool powerSwitchState = HIGH;
    bool pumpStatus = false;
    // Child lock.
    unsigned long childLockSwitchStartTime = 0;
    bool leftArrowSwitchState = HIGH;
    bool childLock = false;
    // Bypass.
    unsigned long bypassSwitchStartTime = 0;
    bool rightArrowSwitchState = HIGH;
    bool bypass = false;
    // timer
    unsigned long timer = 0;
    unsigned long timerStartTime = 0;
    unsigned long timerSettingStartTime = 0;
    unsigned long timerDisplayBlinkTime = 0;
    uint8_t powerSwitchClickCnt = 0;
    bool hideTimer = true;
  };
  struct TimeConversionResult {
    uint8_t minutes;
    uint8_t seconds;
  };
  int relaySwitchPin, leftArrowSwitchPin, powerSwitchPin, rightArrowSwitchPin, voltageSensorPin, currentSensorPin;
  Display display;
  ControlBox(
    int relaySwitchPin = 4, 
    int leftArrowSwitchPin = 7, 
    int powerSwitchPin = 6, 
    int rightArrowSwitchPin = 5, 
    int voltageSensorPin = A0, 
    int currentSensorPin = A1);
  void setup();
  void loop();
  ControlBoxState getState() const;
  uint8_t getWaterLevel(uint16_t distance = UINT16_MAX);
  void setWaterDistance(uint16_t distance);
  void setHeartBeat(bool heartBeat);
  void changePumpStatus(bool status = false);
  void togglePower();
  void toggleChildLock();
  void toggleBypass();
  void cancelTimer();
  void startTimer();
  TimeConversionResult convertMiliToMinSec();
  float measureCurrent();
  float measureVoltage();
  void measureWattPower(bool force = false);
private:
  ControlBoxState state;
  ZMPT101B voltageSensor;
  void checkHeartBeat();
  void onClickPowerSwitch();
  void onClicLeftArrowSwitch();
  void onClicRightArrowSwitch();
  void autoPowerOnOff();
  void onTimerActivate();
  void addTimer();
  void minusTimer();
  void resetPowerSwitchState(uint8_t clickCnt = 0);
  void onTimerSwitchOff();
};
#endif