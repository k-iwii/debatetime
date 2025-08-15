#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

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
String formatSelected = "";
int prevSpeech[2] = { 0, 0 };

// debate formats
String formatA = "BP";
String formatB = "WS";
int ATime[4][2] = { { 0, 30 }, { 4, 30 }, { 5, 0 }, { 5, 15 } };  // {protected ends, protected starts, speech ends, grace ends}
int BTime[4][2] = { { 1, 0 }, { 7, 0 }, { 8, 0 }, { 8, 15 } };    // {protected ends, protected starts, speech ends, grace ends}

void setup() {
  // arduino pin setup
  pinMode(buttonA, INPUT_PULLUP);
  pinMode(buttonB, INPUT_PULLUP);
  pinMode(LEDred, OUTPUT);    // Red
  pinMode(LEDgreen, OUTPUT);  // Green
  pinMode(LEDblue, OUTPUT);   // Blue
  pinMode(switchPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(switchPin), switchISR, FALLING);

  // serial monitor setup
  Serial.begin(9600);
  while (!Serial)
    ;

  // display setup
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  display.clearDisplay();

  // BLE connection setup
  String uniqueName = "ESP32_" + String(millis() & 0xFFFF);  // last 16 bits of millis
  Serial.println("Device Name: " + uniqueName);

  BLEDevice::init(uniqueName.c_str());

  BLEServer* pServer = BLEDevice::createServer();
  BLEService* pService = pServer->createService(SERVICE_UUID);
  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  Serial.println("BLE advertising started!");
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
  Serial.println("timer started");

  formatSelectionScreen();
  formatSelected = formatSelectionLogic();
  Serial.println(formatSelected);

  confirmationScreen();

  timing();

  speechComplete();
  started = false;
}

// 1: format selection screen
void formatSelectionScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  textPrintln("");
  headerPrintln("DebateTime");
  textPrintln("\nSelect debate format:");
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

// 2: confirmation screen
void confirmationScreen() {
  if (programOff) return;
  display.clearDisplay();

  // stopwatch
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print(" ");  // helps centre the numbers
  headerPrintln("    " + formatSelected);

  // format info
  textPrintln("");
  switch (formatSelected.equals(formatA)) {
    case true:
      textPrintln("Speech: " + String(ATime[2][0]) + ":" + formatSecs(ATime[2][1]));
      textPrintln("POIs: " + String(ATime[0][0]) + ":" + formatSecs(ATime[0][1]) + " - " + String(ATime[1][0]) + ":" + formatSecs(ATime[1][1]));
      textPrintln("Grace: " + formatSecs(ATime[3][1] - ATime[2][1]) + "s");
      break;
    case false:
      textPrintln("Speech: " + String(BTime[2][0]) + ":" + formatSecs(BTime[2][1]));
      textPrintln("POIs: " + String(BTime[0][0]) + ":" + formatSecs(BTime[0][1]) + " - " + String(BTime[1][0]) + ":" + formatSecs(BTime[1][1]));
      textPrintln("Grace: " + formatSecs(BTime[3][1] - BTime[2][1]) + "s");
      break;
  }

  // instructions
  textPrintln("\n  Press A to start");

  setLEDColour(255, 100, 100);
  display.display();

  while (true) {
    if (switchPressed) {
      turnOff();
      return;
    }
    if (programOff) return;

    if (digitalRead(buttonA) == LOW) {
      Serial.println("format A, " + formatA + ", was selected");
      delay(300);
      return;
    }
  }
}

// 3: timer screen
void timing() {
  if (programOff) return;
  timerScreen(0, 0);
  delay(300);

  int protectedSecs[2] = {
    (formatSelected == formatA) ? (ATime[0][0] * 60 + ATime[0][1]) : (BTime[0][0] * 60 + BTime[0][1]),
    (formatSelected == formatA) ? (ATime[1][0] * 60 + ATime[1][1]) : (BTime[1][0] * 60 + BTime[1][1])
  };
  int totalSecs = (formatSelected == formatA) ? (ATime[2][0] * 60 + ATime[2][1]) : (BTime[2][0] * 60 + BTime[2][1]);
  int graceSecs = (formatSelected == formatA) ? (ATime[3][0] * 60 + ATime[3][1]) : (BTime[3][0] * 60 + BTime[3][1]);

  Serial.println("graceSecs: " + graceSecs);

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
        if (elapsedSecs <= protectedSecs[0] || elapsedSecs >= protectedSecs[1])  // protected time
          setLEDColour(200, 255, 0);                                             // light green
        else                                                                     // speech time
          setLEDColour(0, 255, 0);                                               // green
      } else if (elapsedSecs <= graceSecs)                                       // grace time
        setLEDColour(255, 90, 0);                                                // orange
      else                                                                       // speech over
        setLEDColour(255, 0, 0);                                                 // red
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

// 4: speech over screen
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

  started = false;
  switchPressed = false;
  programOff = true;  // signal loops to stop

  setLEDColour(0, 0, 0);
  display.clearDisplay();
  display.display();

  prevSpeech[0] = 0;
  prevSpeech[1] = 0;
  formatSelected = "";
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