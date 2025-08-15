#ifndef MYBLECALLBACKS_H
#define MYBLECALLBACKS_H

#include <BLEServer.h>

// variables & functions defined in the main code
extern bool blinkingBlue;
extern void setLEDColour(int red, int green, int blue);

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    //Serial.println("BLE client connected!");
    blinkingBlue = false; 
    setLEDColour(0, 0, 255);
  }
};

#endif