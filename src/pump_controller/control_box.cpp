#include "control_box.h"

void ControlBox::init(){
  display.initDisplay();
}

ControlBox::ControlBoxState ControlBox::getState()const{
  return state;
}

void ControlBox::checkHeartBeat(){
    if(millis() - state.lastUpdatedHeartBeat >= 3000){
      setHeartBeat(false);
    }
}

void ControlBox::setWaterDistance(uint16_t distance){
  uint16_t changeWaterLevel = abs(distance - state.waterDistance);
  if(changeWaterLevel > 30 && state.waterDistanceErrorCnt <= 2 && state.waterDistance != UINT16_MAX){
    state.waterDistanceErrorCnt ++;
    return;
  }
  state.waterDistanceErrorCnt = 0;
  state.waterDistance = distance;
  display.setWaterLevel(distance);
}

void ControlBox::setHeartBeat(bool heartBeat){
  state.heartBeat = heartBeat;
  state.lastUpdatedHeartBeat = millis();
  display.drawWaterTankHearBeat(heartBeat);
}