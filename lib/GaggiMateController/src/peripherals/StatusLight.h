#pragma once

#include <cmath>
#include <cstdint>

// Pure output state: called by the existing controller loop, with no extra timer/task.
class StatusLight {
  public:
    enum Function : uint32_t { Off = 0, Steam = 1, Brew = 2, Awake = 3 };

    bool update(uint32_t now, uint32_t function, float temperature, float target, bool awake, bool fault) {
        uint8_t requested = 0;
        if (function == Off) {
            ready = false;
        } else if (fault) {
            requested = 1;
        } else if (function == Awake) {
            requested = awake ? 3 : 0;
        } else if (function <= Brew && target > 0.0f && std::isfinite(temperature)) {
            // Enter ready within 1C; return to heating only when more than 2C below target.
            if (temperature >= target - 1.0f)
                ready = true;
            else if (temperature < target - 2.0f)
                ready = false;
            requested = ready ? 3 : 2;
        } else {
            ready = false;
        }
        if (requested != mode) {
            mode = requested;
            startedAt = now;
        }
        const uint32_t elapsed = now - startedAt;
        const uint32_t phase = elapsed % 1350;
        return mode == 1   ? phase < 600 && (phase / 100) % 2 == 0
               : mode == 2 ? (elapsed / 500) % 2 == 0
                           : mode == 3;
    }

  private:
    uint32_t startedAt = 0;
    uint8_t mode = 0;
    bool ready = false;
};
