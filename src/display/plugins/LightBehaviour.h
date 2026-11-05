#pragma once
#include <LedEffect.h>
#include <cmath>
#include <cstdint>

// The display owns process state and resolves the user's light function.
class LightBehaviour {
  public:
    LedEffect update(uint32_t function, float temperature, float target, bool awake, bool brewing) {
        if (function != previousFunction) {
            ready = false;
            previousFunction = function;
        }
        if (function == 3)
            return awake ? LedEffect::On : LedEffect::Off;
        if (function == 4)
            return brewing ? LedEffect::On : LedEffect::Off;
        if ((function != 1 && function != 2 && function != 5 && function != 6) || !awake || target <= 0 || !std::isfinite(temperature)) {
            ready = false;
            return LedEffect::Off;
        }
        // Enter ready within 1C; resume heating only when more than 2C below target.
        if (temperature >= target - 1.0f)
            ready = true;
        else if (temperature < target - 2.0f)
            ready = false;
        return ready ? LedEffect::On : function >= 5 ? LedEffect::Flashing : LedEffect::Breathing;
    }

  private:
    uint32_t previousFunction = 0;
    bool ready = false;
};
