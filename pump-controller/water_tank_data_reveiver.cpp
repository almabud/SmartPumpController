#include "water_tank_data_receiver.h"

const byte WaterTankDataController::aes_key[16] = { 'B', 'd', '@', 4, 6, 4, 2, 5, 8, 's', 'c', 'r', 'e', 't', 't', 't' };

WaterTankDataController::WaterTankDataController(ControlBox& controlBox)
  : controlBox(controlBox), driver(2000, 12, -1, -1) {
}

void WaterTankDataController::setup() {
  driver.init();
  // if (!driver.init())  // Initialize ASK driver
  //   Serial.println("init failed");

  // Initialize AES
  aes128.setKey(aes_key, sizeof(aes_key));
}

void WaterTankDataController::loop() {
  receiveWaterDistance();
}

void WaterTankDataController::receiveWaterDistance() {
  uint8_t buf[16];
  uint8_t buflen = sizeof(buf);

  if (driver.recv(buf, &buflen)) {
    int distance;
    unsigned long timestamp;

    if (decryptAndValidate(buf, distance, timestamp)) {
      controlBox.setWaterDistance(distance);
      controlBox.setHeartBeat(true);
    } else {
      controlBox.setHeartBeat(false);
    }
  }
}

// Private
// Function to decrypt and validate data
bool WaterTankDataController::decryptAndValidate(byte* data, int& distance, unsigned long& timestamp) {
  // Create a temporary buffer for decryption
  byte temp[16];
  memcpy(temp, data, 16);

  // Decrypt the data
  aes128.decryptBlock(temp, temp);

  // Extract water level (first 4 bytes)
  distance = ((long)temp[0] << 24) | ((long)temp[1] << 16) | ((long)temp[2] << 8) | (long)temp[3];

  // Basic validation: water level should be reasonable
  if (distance < 0 || distance > 1023) {
    return false;
  }

  return true;
}