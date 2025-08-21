#ifndef SIMPLELEDCONTROLLER_H
#define SIMPLELEDCONTROLLER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define PIN_WS2812B 16  // The ESP32 pin GPIO16 connected to WS2812B
#define NUM_PIXELS 7   // The number of LEDs (pixels) on WS2812B LED strip


class SimpleLedController {
  public:
    SimpleLedController();
    void setup();
    bool isAvailable();
    void setChannel(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    void disable();

  private:
    bool initialize();

    Adafruit_NeoPixel *ws2812b = nullptr;
    bool initialized = false;

};

#endif // SIMPLELEDCONTROLLER_H
