#include "SimpleLedController.h"


SimpleLedController::SimpleLedController() { this->ws2812b = new Adafruit_NeoPixel(NUM_PIXELS, PIN_WS2812B, NEO_GRB + NEO_KHZ800); }


void SimpleLedController::setup() {
    this->initialize();  // initialize WS2812B strip object (REQUIRED)
    this->disable();  // set all pixel colors to 'off'. It only takes effect if pixels.show() is called

}

bool SimpleLedController::isAvailable() { return this->initialize(); }
void SimpleLedController::setChannel(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {

    if (w<40){ w=40;}
    this->ws2812b->setBrightness(w);

    for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {         
        this->ws2812b->setPixelColor(pixel, this->ws2812b->Color(r, g, b)); 
    }
    this->ws2812b->show(); 
}
void SimpleLedController::disable() { 
    this->ws2812b->clear(); 
    this->ws2812b->show(); 
}

bool SimpleLedController::initialize() {
    if (this->initialized) {
        return true;
    }
    bool retval = this->ws2812b->begin();
    if (!retval) {
        ESP_LOGE("SimpleLedController", "Failed to initialize WS2812B");
    }
    this->initialized = retval;
    return retval;
}
