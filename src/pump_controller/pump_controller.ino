#include <RH_ASK.h>
#include <SPI.h>  // Not actually used but needed to compile
#include <Crypto.h>
#include <AES.h>
#include <string.h>
#include "display.h"

RH_ASK driver(2000, 4, 4);
Display display;
// AES128 instance
AES128 aes128;

byte aes_key[] = { 'B', 'd', '@', 4, 6, 4, 2, 5, 8, 's', 'c', 'r', 'e', 't', 't', 't' };  // 16-byte key for AES-128
// Buffer for encrypted data
byte encrypted[16];


void setup() {
  Serial.begin(9600);  // Starts the serial communication

  if (!driver.init())  // Initialize ASK driver
    Serial.println("init failed");

  // Initialize AES
  aes128.setKey(aes_key, sizeof(aes_key));
  display.initDisplay();
}

// Function to decrypt and validate data
bool decryptAndValidate(byte* data, int& distance, unsigned long& timestamp) {
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

void receiveWaterDistance() {
  uint8_t buf[16];
  uint8_t buflen = sizeof(buf);

  if (driver.recv(buf, &buflen)) {
    int distance;
    unsigned long timestamp;

    if (decryptAndValidate(buf, distance, timestamp)) {
      Serial.print("Water Level: ");
      Serial.println(distance);
    } else {
      Serial.println("Invalid data received");
    }
  }
}

uint8_t i = 0;

void loop() {
  // Receive any new data from water level measurement controller.
  // receiveWaterDistance();
  // delay(1000);
  // display.();
  // delay(10);
  display.setWaterLevel(1000);
  display.drawTodayHistory(false, 100, 1500, 1500);
  display.drawPumpStatus();
  delay(3000);
  display.setWaterLevel(800);
  delay(3000);
  display.setWaterLevel(300);
  delay(3000);
  display.setWaterLevel(0);
  display.drawPumpStatus(true);
  display.drawTodayHistory(true);
  display.drawBypass(true);
  display.drawChildLock(true);
  display.drawWaterTankHearBeat(true);
  display.drawTimer(49, 80);
  delay(3000);
  i++;
}
