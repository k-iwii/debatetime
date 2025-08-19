#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLEAdvertising.h>
#include "BLEConnectionCallbacks.h"
#include "BLEFormats.h"

#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define NUMFLAKES 10  // Number of snowflakes in the animation example
#define LOGO_HEIGHT 16
#define LOGO_WIDTH 16
#define SERVICE_UUID "d4b24792-2610-4be4-97fa-945af5cf144e"  // change this

// input pins; all digital
int buttonA = 5, buttonB = 6;
int LEDred = 9, LEDgreen = 10, LEDblue = 11;
int switchPin = 4;

// allowing the switch to interrupt other operations
volatile bool switchPressed = false;  // flag to indicate switch turned off
volatile bool programOff = false;
void IRAM_ATTR switchISR() {
  switchPressed = true;
  programOff = true;  // signal the whole program to stop
}

// variables
boolean started = false;
String selectedFormatName = "";
int (*selectedFormatTime)[2];
int prevSpeech[2] = { 0, 0 };
bool replySpeech = false;
bool blinkingBlue = false; // led blinking flag

// bluetooth
BLEServer* pServer;
BLEAdvertising* pAdvertising;

// debate formats
String formatA = "BP";
String formatB = "WS";
int ATime[5][2] = { { 0, 30 }, { 4, 30 }, { 5, 0 }, { 0, 15 }, {0, 0} };  // {protected ends, protected starts, speech len, grace len, reply len}
int BTime[5][2] = { { 1, 0 }, { 7, 0 }, { 8, 0 }, { 0, 15 }, {0, 15} };    // {protected ends, protected starts, speech len, grace len, reply len}

void setup() {
  // arduino pin setup
  pinMode(buttonA, INPUT_PULLUP);
  pinMode(buttonB, INPUT_PULLUP);
  pinMode(LEDred, OUTPUT);    // Red
  pinMode(LEDgreen, OUTPUT);  // Green
  pinMode(LEDblue, OUTPUT);   // Blue
  pinMode(switchPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(switchPin), switchISR, FALLING);

  // display setup
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  display.clearDisplay();

  // serial monitor setup
  Serial.begin(115200);
  while (!Serial)
    ;

  // BLE connection setup
  String uniqueName = "ESP32_" + String(millis() & 0xFFFF);
  Serial.println("Device Name: " + uniqueName);
  BLEDevice::init(uniqueName.c_str());

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = pServer->createService(SERVICE_UUID);
  pService->start();

  // advertising prepared, but not started
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->stop();

  /*Serial.end();
  Serial.begin(9600); // FIX THIS SERIAL PROBLEm
  while (!Serial)
    ;*/
}

void loop() {
  if (digitalRead(switchPin)) {
    programOff = false;
    if (!started)
      timerLogic();
  } else {
    programOff = true;
    turnOff();
  }
}

// ------------------------ LOGIC & DISPLAY ------------------------
void timerLogic() {
  started = true;
  reset();
  //Serial.println("timer started");

  landingScreen();
  landingScreenLogic();

  formatSelectionScreen();
  selectedFormatName = formatSelectionLogic();
  if (selectedFormatName.equals(formatA))
    selectedFormatTime = ATime;
  else
    selectedFormatTime = BTime;

  confirmationScreen();

  timing();

  speechComplete();
  started = false;
}

// 1: landing / bluetooth page
void landingScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  textPrintln("---------------------");
  headerPrintln("DebateTime");
  textPrintln("---------------------");
  textPrintln("\nPress A to begin");
  textPrintln("Press B for Bluetooth");
  display.display();
}

void landingScreenLogic() {
  bool bluetoothStarted = false;

  while (true) {
    if (switchPressed) {
      turnOff();
      return;
    }
    if (programOff) return;

    if (digitalRead(buttonA) == LOW) {
      delay(300);

      // stop BLE advertising
      if (bluetoothStarted) {
        pAdvertising->stop();
        Serial.println("BLE advertising stopped");

        // disconnect all connected clients
        if (pServer->getConnectedCount() > 0) {
          pServer->disconnect(0);  // 0 = first connected device
          Serial.println("BLE client disconnected by user");
        }

        bluetoothStarted = false;
        blinkingBlue = false;
        setLEDColour(0, 0, 0);
      }

      return;
    } else if (digitalRead(buttonB) == LOW && !bluetoothStarted) {
      BLEDevice::getAdvertising()->start();
      Serial.println("BLE advertising started from landing screen!");
      bluetoothStarted = true;
      blinkingBlue = true;
    }

    if (bluetoothStarted && blinkingBlue) LEDblink(0, 0, 255);
  }
}

// 2: format selection screen
void formatSelectionScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  textPrintln("---------------------\nSelect debate format:\n---------------------");
  textPrintln("\nA: " + formatA);
  textPrintln("B: " + formatB);
  display.display();
  Serial.println("displayed format selection screen");

  setLEDColour(0, 0, 0);
}

String formatSelectionLogic() {
  while (true) {
    if (switchPressed) {
      turnOff();
      return "";
    }
    if (programOff) return "";

    if (digitalRead(buttonA) == LOW) {
      Serial.println("format A, " + formatA + ", was selected");
      delay(300);
      return formatA;
    } else if (digitalRead(buttonB) == LOW) {
      Serial.println("format B, " + formatB + ", was selected");
      delay(300);
      return formatB;
    }
  }
}


// 3: confirmation screen
void confirmationScreen() {
  if (programOff) return;
  display.clearDisplay();

  // stopwatch
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print(" ");  // helps centre the numbers
  headerPrintln("    " + selectedFormatName);

  // format info
  textPrintln("");
  textPrintln("Speech: " + String(selectedFormatTime[2][0]) + ":" + formatSecs(selectedFormatTime[2][1]));
  textPrintln("POIs: " + String(selectedFormatTime[0][0]) + ":" + formatSecs(selectedFormatTime[0][1]) + " - " + String(selectedFormatTime[1][0]) + ":" + formatSecs(selectedFormatTime[1][1]));
  textPrintln("Grace: " + formatSecs(selectedFormatTime[3][1] - selectedFormatTime[2][1]) + "s");

  // instructions
  if (selectedFormatTime[4][0] == 0 && selectedFormatTime[4][1] == 0)
    textPrintln("\n  Press A to start");
  else
    textPrintln("\nA: Speech || B: Reply");

  setLEDColour(255, 100, 100);
  display.display();

  while (true) {
    if (switchPressed) {
      turnOff();
      return;
    }
    if (programOff) return;

    if (digitalRead(buttonA) == LOW) {
      delay(300);
      return;
    }
    if (digitalRead(buttonB) == LOW && !(selectedFormatTime[4][0] == 0 && selectedFormatTime[4][1] == 0)) {
      replySpeech = true;
      delay(300);
      return;
    }
  }
}

// 4: timer screen
void timing() {
  if (programOff) return;
  timerScreen(0, 0);
  delay(300);

  int protectedSecs[2] = {
    selectedFormatTime[0][0] * 60 + selectedFormatTime[0][1],
    selectedFormatTime[1][0] * 60 + selectedFormatTime[1][1]
  };

  int totalSecs = replySpeech ? selectedFormatTime[4][0] * 60 + selectedFormatTime[4][1] : selectedFormatTime[2][0] * 60 + selectedFormatTime[2][1];
  int graceSecs = totalSecs + (selectedFormatTime[3][0] * 60 + selectedFormatTime[3][1]);
  Serial.println("graceSecs: " + String(graceSecs));
  Serial.println("totalSecs: " + String(totalSecs));

  unsigned long startTime = millis();
  int elapsedSecs = 0;
  while (true) {
    if (switchPressed) {
      turnOff();
      return;
    }
    if (programOff) return;

    unsigned long currentMillis = millis();
    int newElapsedSecs = (currentMillis - startTime) / 1000;

    if (newElapsedSecs > elapsedSecs) {
      elapsedSecs = newElapsedSecs;

      int mins = elapsedSecs / 60;
      int secs = elapsedSecs % 60;
      timerScreen(secs, mins);

      if (elapsedSecs <= totalSecs) {
        if (!replySpeech && (elapsedSecs <= protectedSecs[0] || elapsedSecs >= protectedSecs[1]))  // protected time
          setLEDColour(200, 255, 0);       // light green
        else                               // speech time
          setLEDColour(0, 255, 0);         // green
      } else if (elapsedSecs <= graceSecs) // grace time
        setLEDColour(255, 90, 0);          // orange
      else {                                // speech over
        setLEDColour(255, 0, 0);           // red
        Serial.println("replySpeech: " + String(replySpeech));
        Serial.println("elapsedSecs: " + String(elapsedSecs));
      }
    }

    // check for button presses while the timer is running
    if (digitalRead(buttonA) == LOW) {
      Serial.println("Pause button pressed");
      delay(1000);

      // pause until A is pressed again
      while (true) {
        if (switchPressed) { 
          turnOff();
          return;
        }
        if (programOff) return;

        if (digitalRead(buttonA) == LOW) {
          Serial.println("Play button pressed");
          delay(300);
          break;
        }

        if (digitalRead(buttonB) == LOW) {
          Serial.println("End speech during pause");
          prevSpeech[0] = elapsedSecs / 60;
          prevSpeech[1] = elapsedSecs % 60;
          waveClearScreen();
          return;  // completely exit the timing() function
        }
      }
      startTime = millis() - elapsedSecs * 1000;  // resume timer
    }

    if (digitalRead(buttonB) == LOW) {
      Serial.println("End speech early");
      prevSpeech[0] = elapsedSecs / 60;
      prevSpeech[1] = elapsedSecs % 60;
      break;
    }
  }

  delay(300);
  waveClearScreen();
}

void timerScreen(int secs, int mins) {
  display.clearDisplay();

  display.setCursor(0, 0);
  textPrintln("\n  -----------------");  // upper border
  display.print(" ");                    //helps centre the numbers
  headerPrintln("   " + String(mins) + ":" + formatSecs(secs));
  textPrintln("  -----------------");  // lower border

  textPrintln("\nA: Pause/play");
  textPrintln("B: End");

  display.display();
}

// 5: speech over screen
void speechComplete() {
  if (programOff) return;
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  textPrintln("");
  headerPrintln("   Done");
  textPrintln("\nYou spoke for " + String(prevSpeech[0]) + ":" + formatSecs(prevSpeech[1]) + "!");
  textPrintln("\n Press A to restart");
  display.display();

  while (true) {
    if (switchPressed) {
      turnOff();
      return;
    }
    if (programOff) return;

    if (digitalRead(buttonA) == LOW) {
      delay(500);
      return;
    }
  }
}


// ------------------------ OUTPUT / DISPLAY FUNCTIONS ------------------------
void headerPrintln(String text) {
  display.setTextSize(2);
  display.println(text);
}

void textPrintln(String text) {
  display.setTextSize(1);
  display.println(text);
}

void setLEDColour(int red, int green, int blue) {
  analogWrite(LEDred, red);
  analogWrite(LEDgreen, green);
  analogWrite(LEDblue, blue);
  Serial.println("colour set to " + String(red) + "," + String(green) + "," + String(blue));
}

String formatSecs(int secs) {
  return (secs < 10) ? ("0" + String(secs)) : String(secs);
}

// when switched to off
void turnOff() {
  Serial.println("Switch turned off, exiting loop");
  programOff = true;  // signals loops to stop

  reset();

  // reset display
  setLEDColour(0, 0, 0);
  display.clearDisplay();
  display.display();
}

void reset() {
  // reset flags
  started = false;
  switchPressed = false;
  blinkingBlue = false; 

  // reset variables
  prevSpeech[0] = 0;
  prevSpeech[1] = 0;
  selectedFormatName = "";
  selectedFormatTime = nullptr;
  replySpeech = false;
}

// clearing screen animation after speech ends
void waveClearScreen() {
  // Get a snapshot of the screen buffer
  display.display();  // ensure content is pushed to screen
  setLEDColour(0, 0, 0);

  for (int x = 0; x < SCREEN_WIDTH; x++) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
      display.drawPixel(x, y, BLACK);
    }
    display.display();  // push the current column clear
    delay(3);
  }
}

// non-blocking LED blinking function
unsigned long previousBlinkMillis = 0;
bool ledState = false;
void LEDblink(int red, int green, int blue) {
  const unsigned long blinkInterval = 300; // milliseconds ON or OFF time
  unsigned long currentMillis = millis();

  if (currentMillis - previousBlinkMillis >= blinkInterval) {
    previousBlinkMillis = currentMillis;
    ledState = !ledState;

    if (ledState) { // on
      analogWrite(LEDred, red);
      analogWrite(LEDgreen, green);
      analogWrite(LEDblue, blue);
    } else { // off
      analogWrite(LEDred, 0);
      analogWrite(LEDgreen, 0);
      analogWrite(LEDblue, 0);
    }
  }
}