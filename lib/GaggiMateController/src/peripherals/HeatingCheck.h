#ifndef HEATINGCHECK_H
#define HEATINGCHECK_H

#include <algorithm>
#include <cmath>
#include <cstdint>

// Uses cumulative time actually commanded at the SSR, after heater arbitration.
class HeatingCheck {
  public:
    enum class Fault { None, NoRise, UnexpectedRise };
    static constexpr float MIN_RISE_C = 2.0f;
    static constexpr float TARGET_MARGIN_C = 10.0f;
    static constexpr float MAX_OFF_RISE_C = 10.0f;
    static constexpr uint32_t OFF_ALLOWANCE_MS = 15000;
    static constexpr uint32_t SENSOR_SETTLE_MS = 10000;

    explicit HeatingCheck(uint32_t heatingTimeMs = 30000) : heatingTimeMs(heatingTimeMs) {}
    void setHeatingTime(uint32_t ms) { heatingTimeMs = ms; }

    Fault update(uint32_t now, float temperature, float target, uint32_t totalOnMs, bool on) {
        if (!std::isfinite(temperature))
            return Fault::None; // Sensor validation remains the sensor driver's responsibility.
        if (!initialized) {
            initialized = true;
            startedAt = now;
            previousOnMs = totalOnMs;
            lastPoweredAt = now;
            riseBaseline = offMinimum = temperature;
        }
        const uint32_t poweredMs = totalOnMs - previousOnMs;
        previousOnMs = totalOnMs;
        if (on || poweredMs != 0) {
            lastPoweredAt = now;
            offMinimum = temperature;
        } else if (now - startedAt < SENSOR_SETTLE_MS) {
            // Existing thermocouple filtering ramps from zero on startup.
            offMinimum = temperature;
        } else {
            offMinimum = std::min(offMinimum, temperature);
            // Ten degrees of residual rise is allowed after output switches off.
            if (temperature - offMinimum > MAX_OFF_RISE_C)
                return Fault::UnexpectedRise;
        }

        // Near-target maintenance duty need not cause a continuing temperature rise.
        // A long interruption starts a fresh window instead of accumulating unrelated heating periods.
        if (target <= 0 || temperature >= target - TARGET_MARGIN_C || now - lastPoweredAt > OFF_ALLOWANCE_MS) {
            watchingRise = false;
            heatingMs = 0;
            return Fault::None;
        }
        if (!watchingRise) {
            watchingRise = true;
            riseBaseline = temperature;
            heatingMs = 0;
            return Fault::None;
        }
        heatingMs += poweredMs;
        // Compare the end of a complete heating window with its fixed starting temperature.
        // Small oscillations must not move the baseline or repeatedly restart the timer.
        if (heatingMs >= heatingTimeMs) {
            if (temperature < riseBaseline + MIN_RISE_C)
                return Fault::NoRise;
            riseBaseline = temperature;
            heatingMs = 0;
        }
        return Fault::None;
    }

  private:
    uint32_t heatingTimeMs;
    uint32_t startedAt = 0;
    uint32_t previousOnMs = 0;
    uint32_t lastPoweredAt = 0;
    uint32_t heatingMs = 0;
    float riseBaseline = 0;
    float offMinimum = 0;
    bool initialized = false;
    bool watchingRise = false;
};

#endif
