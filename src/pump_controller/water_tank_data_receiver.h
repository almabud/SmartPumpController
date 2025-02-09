#ifndef WATER_TANK_DATA_RECEIVER_H
#define WATER_TANK_DATA_RECEIVER_H
#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h>
#include <Crypto.h>
#include <AES.h>
#include <string.h>
#include "control_box.h"

class WaterTankDataController {
public:
  WaterTankDataController(ControlBox &controlBox);
  void init();
  void receiveWaterDistance();
private:
  ControlBox& controlBox;
  // RH_ASK driver(2000, 4, 4);
  RH_ASK driver;
  // AES128 instance
  AES128 aes128;
  static const byte aes_key[16];  // 16-byte key for AES-128

  bool decryptAndValidate(byte* data, int& distance, unsigned long& timestamp);
};

#endif