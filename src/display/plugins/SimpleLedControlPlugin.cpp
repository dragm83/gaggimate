#include "SimpleLedControlPlugin.h"
#include "../core/Controller.h"
#include "../core/Event.h"

void SimpleLedControlPlugin::setup(Controller *controller, PluginManager *pluginManager) {
    this->controller = controller;
    pluginManager->on("controller:ready", [this](Event const) { initialized = true; });
    delay(1500);
}

void SimpleLedControlPlugin::loop() {
    if (!initialized) {
        return;
    }
    if (lastUpdate + UPDATE_INTERVAL_SLED < millis()) {
        lastUpdate = millis();
        updateControl();
    }
}

void SimpleLedControlPlugin::updateControl() {
    Settings settings = this->controller->getSettings();
    int mode = this->controller->getMode();
    if (mode == MODE_BREW || mode == MODE_STEAM){
        if (abs(controller->getCurrentTemp()-controller->getTargetTemp())<=2) {
            sendControl(13, 184, 54, 100, 255); 
        } else if ((controller->getCurrentTemp()-controller->getTargetTemp())>3){
            sendControl(217, 24, 24, 90, 255); 

        } else{
            sendControl(32, 96, 117, 40, 255);  
        }       
        return;
    }
    if (this->controller->isActive() && mode == MODE_BREW) {
        sendControl(0, 255, 0, 200, 255); 

        return;
    }
    if (this->controller->getLastProcess() != nullptr && mode == MODE_BREW) {
        sendControl(0, 255, 0, 40, 255);
        return;
    }
    sendControl(settings.getSunriseR(), settings.getSunriseG(), settings.getSunriseB(), settings.getSunriseW(),
                settings.getSunriseExtBrightness());
}

void SimpleLedControlPlugin::sendControl(uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t ext) {
    if (r != last_r || b != last_b || g != last_g || w != last_w){
        this->controller->getClientController()->sendSimpleLedControl(r, g, b, w);
    }

    last_r = r;
    last_g = g;
    last_b = b;
    last_w = w;
    last_ext = ext;
}
