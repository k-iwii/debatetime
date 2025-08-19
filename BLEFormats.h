#ifndef MYBLERECEIVE_H
#define MYBLERECEIVE_H

#include <NimBLEDevice.h>

extern String formatName;         // global variable to store received format name
extern int formatTime[4][2];      // global variable to store received timings

class MyReceiveCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite (NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      // For the name characteristic (simple string)
      if (pCharacteristic->getUUID().toString() == "UUID_FOR_NAME") {
        formatName = String(value.c_str());
        Serial.print("Received format name: ");
        Serial.println(formatName);
      }
      // For the timing characteristic (sent as a comma-separated string)
      else if (pCharacteristic->getUUID().toString() == "UUID_FOR_TIMINGS") {
        // Example format from app: "0,30,4,30,5,0,5,15"
        int index = 0;
        char* token;
        char buf[128];
        value.copy(buf, sizeof(buf));
        token = strtok(buf, ",");
        while (token != NULL && index < 8) {
          int row = index / 2;
          int col = index % 2;
          formatTime[row][col] = atoi(token);
          token = strtok(NULL, ",");
          index++;
        }
        Serial.println("Received timing array:");
        for (int i = 0; i < 4; i++) {
          Serial.print("[");
          Serial.print(formatTime[i][0]);
          Serial.print(",");
          Serial.print(formatTime[i][1]);
          Serial.println("]");
        }
      }
    }
  }
};


void initBLEReceive() {
  BLEDevice::init("MyNanoDevice");
  BLEServer* pServer = BLEDevice::createServer();
  BLEService* pService = pServer->createService("d4b24792-2610-4be4-97fa-945af5cf144e");

  NimBLECharacteristic* pCharacteristic = pService->createCharacteristic(
    "abcd1234-5678-90ab-cdef-1234567890ab",
    NIMBLE_PROPERTY::WRITE
  );
  pCharacteristic->setCallbacks(new MyReceiveCallbacks());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID("d4b24792-2610-4be4-97fa-945af5cf144e");
  pAdvertising->start();
}

#endif