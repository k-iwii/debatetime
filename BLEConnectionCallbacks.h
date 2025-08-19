#ifndef BLECONNECTIONCALLBACKS_H
#define BLECONNECTIONCALLBACKS_H

#include <NimBLEServer.h>
#include <NimBLEDevice.h>

// variables & functions defined in the main code
extern bool blinkingBlue;
extern void setLEDColour(int red, int green, int blue);

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override {
    Serial.println("BLE client connected!");
    blinkingBlue = false;        
    setLEDColour(0, 0, 255);

    /*BLEAddress clientAddress = pServer->getPeerInfo(0)->getAddress();
    Serial.println(clientAddress.toString().c_str());*/
  }

  void onDisconnect (NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override {
    Serial.println("BLE client disconnected!");
    blinkingBlue = true;
  }
};

#endif