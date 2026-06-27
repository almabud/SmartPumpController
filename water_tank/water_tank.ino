#include <RH_ASK.h>
#include <SPI.h> // Not actually used but needed to compile
#include <Crypto.h>
#include <AES.h>
#include <string.h>

RH_ASK driver;
// AES128 instance
AES128 aes128;

byte aes_key[] = {'B', 'd', '@', 4, 6, 4, 2, 5, 8, 's', 'c', 'r', 'e', 't', 't', 't'}; // 16-byte key for AES-128
// Buffer for encrypted data
byte encrypted[16];

const int trigPin = 9;
const int echoPin = 6;

void setup() {
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input
  Serial.begin(9600);       // Starts the serial communication

  if (!driver.init())       // Initialize ASK driver
     Serial.println("init failed");
  
  // Initialize AES
    aes128.setKey(aes_key, sizeof(aes_key));
}

uint16_t calculateDistance(){
  long duration;
  int distance;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;
  return distance;
}

void sendData(uint16_t distance){
    // Clear the buffer
    memset(encrypted, 0, sizeof(encrypted));
    
    // Store water level in first 4 bytes
    encrypted[0] = (distance >> 24) & 0xFF;
    encrypted[1] = (distance >> 16) & 0xFF;
    encrypted[2] = (distance >> 8) & 0xFF;
    encrypted[3] = distance & 0xFF;
    // Rest of the buffer remains as padding (zeros)
    
    // Encrypt the data
    aes128.encryptBlock(encrypted, encrypted);
    driver.send((uint8_t *)encrypted, sizeof(encrypted));
    driver.waitPacketSent();
}

void loop() {
  uint16_t distance = 0;
  for(int i=0; i<5; i++){
    distance += calculateDistance();
    delay(60);
  }
  sendData(distance / 5);
}
