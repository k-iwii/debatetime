# Arduino Debate Timer

A debate timer system built for Arduino ESP32 with OLED display, RGB LED indicator, and BLE connectivity.

## Features

- **Multiple Debate Formats**: Supports high school British Parliamentary and World Schools formats
- **Visual Timer Display**: 128x64 OLED screen shows stopwatch timer
- **LED Status Indicators**: RGB LED changes color based on speech phase:
  - Light green: Protected time (no POIs allowed)
  - Green: Open time (POIs allowed)
  - Orange: Grace period
  - Red: Speech time exceeded
- **Pause/Resume**: Button A pauses and resumes the timer
- **Early End**: Button B ends the speech early
- **BLE Connectivity**: Bluetooth Low Energy for potential remote control
- **Power Switch**: Hardware switch to turn the system on/off

## Hardware Requirements

- ESP32 development board
- SSD1306 OLED display (128x64, I2C)
- RGB LED (common cathode)
- 2 push buttons
- 1 toggle switch

## Pin Configuration

- Button A: Pin 5 (Pause/Play)
- Button B: Pin 6 (End speech)
- RGB LED: Pins 9 (Red), 10 (Green), 11 (Blue)
- Power Switch: Pin 4
- OLED Display: I2C (SDA/SCL)

## Debate Format Timings

### British Parliamentary (BP)
- Speech Length: 5:00
- Protected Time: 0:30 - 4:30 (no POIs)
- Grace Period: 15 seconds

### World Schools (WS)
- Speech Length: 8:00
- Protected Time: 1:00 - 7:00 (no POIs)
- Grace Period: 15 seconds

## Usage

1. Turn on the device using the power switch
2. Select debate format using Button A (BP) or Button B (WS)
3. Confirm selection by pressing Button A
4. Timer starts automatically
5. Use Button A to pause/resume during speech
6. Use Button B to end speech early
7. View final speech time on completion screen

## Dependencies

- Adafruit GFX Library
- Adafruit SSD1306 Library
- ESP32 BLE Arduino Library

## Installation

1. Install the Arduino IDE
2. Add ESP32 board support
3. Install required libraries via Library Manager
4. Upload the code to your ESP32
5. Connect hardware according to pin configuration
