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
extern int colours[4][3];
extern bool receivingChunkedData;
extern String jsonBuffer;
extern const int MAX_JSON_SIZE;
extern const char* CHAR_DATA_UUID;
extern NimBLECharacteristic* pCharData;

class MyReceiveCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
    std::string value = pCharacteristic->getValue();
    std::string uuid = pCharacteristic->getUUID().toString();
    
    if (uuid == CHAR_DATA_UUID) {
      String receivedData = String(value.c_str());
      
      // Check for start marker
      if (receivedData == "START_JSON") {
        Serial.println("Received START_JSON marker - beginning chunked data reception");
        receivingChunkedData = true;
        jsonBuffer = "";
        return;
      }
      
      // Check for end marker
      if (receivedData == "END_JSON") {
        Serial.println("Received END_JSON marker - processing complete JSON");
        receivingChunkedData = false;
        
        if (jsonBuffer.length() > 0) {
          Serial.println("Complete JSON data received:");
          Serial.println(jsonBuffer);
          
          // Parse the complete JSON data
          parseFlutterData(jsonBuffer);
          bleDataReceived = true;
          
          // Clear buffer
          jsonBuffer = "";
        } else {
          Serial.println("Error: No data in buffer when END_JSON received");
        }
        return;
      }
      
      // Handle chunk data
      if (receivingChunkedData) {
        if (jsonBuffer.length() + receivedData.length() <= MAX_JSON_SIZE) {
          jsonBuffer += receivedData;
          Serial.println("Received chunk: " + String(receivedData.length()) + " bytes, total: " + String(jsonBuffer.length()) + " bytes");
        } else {
          Serial.println("Error: JSON buffer overflow, discarding chunk");
          receivingChunkedData = false;
          jsonBuffer = "";
        }
        return;
      }
      
      // Handle single message (backward compatibility)
      Serial.println("Received single JSON message from Flutter app:");
      Serial.println(receivedData);
      parseFlutterData(receivedData);
      bleDataReceived = true;
    }
  }
  
private:
  void parseFlutterData(String jsonData) {
    // Parse new structured JSON from Flutter app
    // Expected format: {"formatA": {"shortName": "BP", "timings": [[0,30],[4,30],[5,0],[5,15],[0,0]]}, "formatB": {...}, "isStopwatch": true}
    
    Serial.println("Parsing new structured JSON...");
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    
    // debate format
    if (parseFormat(jsonData, "formatA", bleFormatA, bleATime)) {
      Serial.println("Successfully parsed Format A: " + bleFormatA);
      printTimings("Format A", bleATime);
    }
    if (parseFormat(jsonData, "formatB", bleFormatB, bleBTime)) {
      Serial.println("Successfully parsed Format B: " + bleFormatB);
      printTimings("Format B", bleBTime);
    }
    
    // timer format
    parseIsStopwatch(jsonData);
    
    // led colours
    parseColours(jsonData);

    pCharData->setValue("PARSE_COMPLETE");
    pCharData->notify();
    Serial.println("notified");
  }

  // debate format
  
  bool parseFormat(String jsonData, String formatKey, String &formatName, int timings[5][2]) {
    String searchKey = "\"" + formatKey + "\":";
    int formatStart = jsonData.indexOf(searchKey);
    if (formatStart == -1) return false;
    
    // Find the opening brace of this format
    int braceStart = jsonData.indexOf("{", formatStart);
    if (braceStart == -1) return false;
    
    // Find the closing brace (simple approach - find next })
    int braceEnd = jsonData.indexOf("}", braceStart);
    if (braceEnd == -1) return false;
    
    String formatSection = jsonData.substring(braceStart + 1, braceEnd);
    
    // Extract shortName
    int nameStart = formatSection.indexOf("\"shortName\":\"") + 13;
    int nameEnd = formatSection.indexOf("\"", nameStart);
    if (nameStart > 12 && nameEnd > nameStart)
      formatName = formatSection.substring(nameStart, nameEnd);
    
    // Extract timings array
    int timingsStart = formatSection.indexOf("\"timings\":[") + 11;
    int timingsEnd = formatSection.indexOf("]]", timingsStart) + 1;
    if (timingsStart > 10 && timingsEnd > timingsStart) {
      String timingsStr = formatSection.substring(timingsStart, timingsEnd);
      return parseTimingsArray(timingsStr, timings);
    }
    
    return false;
  }
  
  bool parseTimingsArray(String timingsStr, int timings[5][2]) {
    int currentTiming = 0;
    int pos = 0;
    
    while (currentTiming < 5 && pos < timingsStr.length()) {
      int openBracket = timingsStr.indexOf("[", pos);
      if (openBracket == -1) break;
      
      int closeBracket = timingsStr.indexOf("]", openBracket);
      if (closeBracket == -1) break;
      
      // extract the pair 
      String pair = timingsStr.substring(openBracket + 1, closeBracket);
      int commaPos = pair.indexOf(",");
      if (commaPos > 0) {
        timings[currentTiming][0] = pair.substring(0, commaPos).toInt();
        timings[currentTiming][1] = pair.substring(commaPos + 1).toInt();
        currentTiming++;
      }
      
      pos = closeBracket + 1;
    }
    
    return currentTiming == 5;  // success if we parsed all 5 timing pairs
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
  
  // stopwatch

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

  // colours
  
  void parseColours(String jsonData) {
    String colourKeys[4] = {"protectedColour", "speechColour", "graceColour", "speechOverColour"};
    
    for (int i = 0; i < 4; i++) {
      parseColourFromJSON(jsonData, colourKeys[i], i);
    }
  }
  
  void parseColourFromJSON(String jsonData, String colourKey, int colourIndex) {
    String searchKey = "\"" + colourKey + "\":";
    int keyStart = jsonData.indexOf(searchKey);
    if (keyStart == -1) {
      Serial.print("Colour key not found: ");
      Serial.println(colourKey);
      return;
    }
    
    int braceStart = jsonData.indexOf("{", keyStart);
    if (braceStart == -1) {
      Serial.println("Opening brace not found");
      return;
    }
    
    int braceEnd = jsonData.indexOf("}", braceStart);
    if (braceEnd == -1) {
      Serial.println("Closing brace not found");
      return;
    }
    
    colours[colourIndex][0] = parseColourValueDirect(jsonData, braceStart, braceEnd, "r");
    colours[colourIndex][1] = parseColourValueDirect(jsonData, braceStart, braceEnd, "g");
    colours[colourIndex][2] = parseColourValueDirect(jsonData, braceStart, braceEnd, "b");
    
    Serial.print(colourKey);
    Serial.print(" parsed: R=");
    Serial.print(colours[colourIndex][0]);
    Serial.print(" G=");
    Serial.print(colours[colourIndex][1]);
    Serial.print(" B=");
    Serial.println(colours[colourIndex][2]);
  }
  
  int parseColourValueDirect(String jsonData, int startPos, int endPos, String component) {
    // Search for "component": within the specified range
    String searchKey = "\"" + component + "\":";
    int searchStart = startPos;
    int keyStart = -1;
    
    // Find the key within the range
    while (searchStart <= endPos - searchKey.length()) {
      keyStart = jsonData.indexOf(searchKey, searchStart);
      if (keyStart == -1 || keyStart > endPos) break;
      searchStart = keyStart + 1;
      if (keyStart < endPos) break;
    }
    
    if (keyStart == -1 || keyStart > endPos) return 0;
    
    int valueStart = keyStart + searchKey.length();
    int valueEnd = jsonData.indexOf(",", valueStart);
    if (valueEnd == -1 || valueEnd > endPos) {
      valueEnd = jsonData.indexOf("}", valueStart);
      if (valueEnd == -1 || valueEnd > endPos) valueEnd = endPos;
    }
    
    // Extract number directly without creating substring
    int value = 0;
    bool foundDigit = false;
    for (int i = valueStart; i < valueEnd; i++) {
      char c = jsonData.charAt(i);
      if (c >= '0' && c <= '9') {
        value = value * 10 + (c - '0');
        foundDigit = true;
      } else if (foundDigit) {
        break; // Stop at first non-digit after finding digits
      }
    }
    
    return value;
  }
};


#endif