#ifndef CONTROL_BOX_H
#define CONTROL_BOX_H

#include <Arduino.h>
#include "display.h"

class ControlBox {
public:
  struct ControlBoxState {
    uint16_t waterDistance = UINT16_MAX;
    uint8_t timer = 0;
    uint16_t pumpRunCnt = 0;
    uint16_t pumpRunTime = 0;
    uint16_t powerConsumption = 0;
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
  };
  int powerSwitchPin = 6;
  int relaySwitchPin = 8;
  int leftArrowSwitchPin = 7;
  int rightArrowSwitchPin = 5;
  Display display;
  ControlBox(int rightArrowSwitchPin = 5, int powerSwitchPin = 6, int leftArrowSwitchPin = 7, int relaySwitchPin = 8);
  void setup();
  void loop();
  ControlBoxState getState() const;
  void checkHeartBeat();
  uint8_t getWaterLevel(uint16_t distance = UINT16_MAX);
  void setWaterDistance(uint16_t distance);
  void setHeartBeat(bool heartBeat);
  void onClickPowerSwitch();
  void onClicLeftArrowSwitch();
  void onClicRightArrowSwitch();
  void changePumpStatus(bool status = false);
  void togglePower();
  void toggleChildLock();
  void toggleBypass();
  void autoPowerOnOff();
private:
  ControlBoxState state;
};
#endif