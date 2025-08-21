#ifndef MYBLERECEIVE_H
#define MYBLERECEIVE_H

#include <NimBLEDevice.h>

// External variables defined in main file
extern bool bleDataReceived;
extern String bleFormatA;
extern String bleFormatB;
extern int bleATime[5][2];
extern int bleBTime[5][2];
extern bool isStopwatch;
extern const char* CHAR_DATA_UUID;

class MyReceiveCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
    std::string value = pCharacteristic->getValue();
    std::string uuid = pCharacteristic->getUUID().toString();
    
    if (uuid == CHAR_DATA_UUID) {
      Serial.println("Received JSON data from Flutter app:");
      Serial.println(value.c_str());
      
      // Parse the JSON-like data from Flutter app
      parseFlutterData(String(value.c_str()));
      bleDataReceived = true;
    }
  }
  
private:
  void parseFlutterData(String jsonData) {
    // Parse new structured JSON from Flutter app
    // Expected format: {"formatA": {"shortName": "BP", "timings": [[0,30],[4,30],[5,0],[5,15],[0,0]]}, "formatB": {...}, "isStopwatch": true}
    
    Serial.println("Parsing new structured JSON...");
    
    // Parse Format A
    if (parseFormat(jsonData, "formatA", bleFormatA, bleATime)) {
      Serial.println("Successfully parsed Format A: " + bleFormatA);
      printTimings("Format A", bleATime);
    }
    
    // Parse Format B  
    if (parseFormat(jsonData, "formatB", bleFormatB, bleBTime)) {
      Serial.println("Successfully parsed Format B: " + bleFormatB);
      printTimings("Format B", bleBTime);
    }
    
    // Parse isStopwatch
    parseIsStopwatch(jsonData);
  }
  
  bool parseFormat(String jsonData, String formatKey, String &formatName, int timings[5][2]) {
    // Find the format section: "formatA": { or "formatB": {
    String searchKey = "\"" + formatKey + "\":";
    int formatStart = jsonData.indexOf(searchKey);
    Serial.println("formatStart: " + String(formatStart));
    if (formatStart == -1) return false;
    
    // Find the opening brace of this format
    int braceStart = jsonData.indexOf("{", formatStart);
    Serial.println("braceStart: " + String(braceStart));
    if (braceStart == -1) return false;
    
    // Find the closing brace (simple approach - find next })
    int braceEnd = jsonData.indexOf("}", braceStart);
    Serial.println("braceEnd: " + String(braceEnd));
    if (braceEnd == -1) return false;
    
    String formatSection = jsonData.substring(braceStart + 1, braceEnd);
    Serial.println("formatSection: " + formatSection);
    
    // Extract shortName
    int nameStart = formatSection.indexOf("\"shortName\":\"") + 13;
    int nameEnd = formatSection.indexOf("\"", nameStart);
    if (nameStart > 12 && nameEnd > nameStart) {
      formatName = formatSection.substring(nameStart, nameEnd);
      Serial.println("formatName: " + formatName);
    }
    
    // Extract timings array
    int timingsStart = formatSection.indexOf("\"timings\":[") + 11;
    int timingsEnd = formatSection.indexOf("]]", timingsStart) + 1;
    if (timingsStart > 10 && timingsEnd > timingsStart) {
      String timingsStr = formatSection.substring(timingsStart, timingsEnd);
      Serial.println("timingsStr: " + timingsStr);
      return parseTimingsArray(timingsStr, timings);
    }
    
    return false;
  }
  
  bool parseTimingsArray(String timingsStr, int timings[5][2]) {
    int currentTiming = 0;
    int pos = 0;
    
    while (currentTiming < 5 && pos < timingsStr.length()) {
      // Find opening bracket
      int openBracket = timingsStr.indexOf("[", pos);
      Serial.println("openBracket: " + String(openBracket));
      if (openBracket == -1) break;
      
      // Find closing bracket
      int closeBracket = timingsStr.indexOf("]", openBracket);
      Serial.println("closeBracket: " + String(closeBracket));
      if (closeBracket == -1) break;
      
      // Extract the pair like "0,30"
      String pair = timingsStr.substring(openBracket + 1, closeBracket);
      Serial.println("pair: " + pair);
      int commaPos = pair.indexOf(",");
      if (commaPos > 0) {
        timings[currentTiming][0] = pair.substring(0, commaPos).toInt();
        timings[currentTiming][1] = pair.substring(commaPos + 1).toInt();
        currentTiming++;
      }
      
      pos = closeBracket + 1;
    }
    
    return currentTiming == 5;  // Success if we parsed all 5 timing pairs
  }
  
  void printTimings(String formatName, int timings[5][2]) {
    Serial.println(formatName + " timings:");
    Serial.println("  Protected start: " + String(timings[0][0]) + ":" + formatSecs(timings[0][1]));
    Serial.println("  Protected end: " + String(timings[1][0]) + ":" + formatSecs(timings[1][1]));
    Serial.println("  Speech length: " + String(timings[2][0]) + ":" + formatSecs(timings[2][1]));
    Serial.println("  Grace time: " + String(timings[3][0]) + ":" + formatSecs(timings[3][1]));
    Serial.println("  Reply length: " + String(timings[4][0]) + ":" + formatSecs(timings[4][1]));
  }
  
  String formatSecs(int secs) {
    return (secs < 10) ? ("0" + String(secs)) : String(secs);
  }
  
  void parseIsStopwatch(String jsonData) {
    String searchKey = "\"isStopwatch\":";
    int keyStart = jsonData.indexOf(searchKey);
    if (keyStart != -1) {
      int valueStart = keyStart + searchKey.length();
      
      // skip whitespace
      while (valueStart < jsonData.length() && (jsonData.charAt(valueStart) == ' ' || jsonData.charAt(valueStart) == '\t'))
        valueStart++;
      
      // check value
      if (jsonData.substring(valueStart, valueStart + 4).equals("true")) {
        isStopwatch = true;
        Serial.println("isStopwatch set to: true");
      } else if (jsonData.substring(valueStart, valueStart + 5).equals("false")) {
        isStopwatch = false;
        Serial.println("isStopwatch set to: false");
      } else
        Serial.println("Could not parse isStopwatch value");
    } else
      Serial.println("isStopwatch field not found in JSON");
  }
};



#endif