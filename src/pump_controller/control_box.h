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
    bool pumpSwitchStatus = false;
    bool childLock = true;
    bool bypass = false;
    bool heartBeat = false;
    unsigned long lastUpdatedHeartBeat = 0;
    // Power switch.
    unsigned long powerSwitchStartTime = 0;
    bool powerSwitchState = HIGH;
  };
  const int powerSwitchPin = 6;
  Display display;
  void setup();
  void loop();
  ControlBoxState getState() const;
  void checkHeartBeat();
  uint8_t getWaterLevel(uint16_t distance = UINT16_MAX);
  void setWaterDistance(uint16_t distance);
  void setHeartBeat(bool heartBeat);
  void powerSwitch();
  void togglePower();
private:
  ControlBoxState state;
};
#endif