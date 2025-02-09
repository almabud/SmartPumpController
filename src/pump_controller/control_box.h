#ifndef CONTROL_BOX_H
#define CONTROL_BOX_H

#include <Arduino.h>
#include "display.h"

class ControlBox {
public:
  struct ControlBoxState {
    uint16_t waterDistance = UINT16_MAX;
    uint8_t waterDistanceErrorCnt = 0;
    uint8_t timer = 0;
    uint16_t pumpRunCnt = 0;
    uint16_t pumpRunTime = 0;
    uint16_t powerConsumption = 0;
    bool pumpSwitchStatus = false;
    bool childLock = true;
    bool bypass = false;
    bool heartBeat = false;
    unsigned long lastUpdatedHeartBeat = 0;
  };
  Display display;
  void init();
  ControlBoxState getState() const;
  void checkHeartBeat();
  void setWaterDistance(uint16_t distance);
  void setHeartBeat(bool heartBeat);
private:
  ControlBoxState state;
};
#endif