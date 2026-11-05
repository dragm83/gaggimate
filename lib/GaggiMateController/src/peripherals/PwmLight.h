#pragma once
#include <Arduino.h>
#include <LedEffect.h>
#include <atomic>
#include <cmath>

// LEDC generates the carrier; the existing controller loop advances the envelope.
class PwmLight {
  public:
    PwmLight(uint8_t pin, uint8_t channel) : pin(pin), channel(channel) {}
    void setup() {
        ledcSetup(channel, 5000, 8);
        ledcWrite(channel, 0);
        ledcAttachPin(pin, channel);
    }
    void set(LedEffect effect, uint8_t brightness) {
        command.store((static_cast<uint16_t>(effect) << 8) | brightness);
    }
    void stop() { command.fetch_and(0xFF); }
    void update(uint32_t now, bool fault = false, bool enabled = true) {
        const uint16_t value = command.load();
        const uint8_t brightness = value & 0xFF;
        const uint8_t mode = fault ? 4 : enabled ? value >> 8 : 0;
        if (mode != previousMode) {
            previousMode = mode;
            startedAt = now;
        }
        const uint32_t elapsed = now - startedAt;
        uint8_t duty = 0;
        if (mode == static_cast<uint8_t>(LedEffect::On))
            duty = brightness;
        else if (mode == static_cast<uint8_t>(LedEffect::Flashing))
            duty = (elapsed / 500) % 2 == 0 ? brightness : 0;
        else if (mode == static_cast<uint8_t>(LedEffect::Breathing))
            duty = static_cast<uint8_t>(brightness * (0.5f - 0.5f * std::cos((elapsed % 3000) * 6.2831853f / 3000)));
        else if (mode == 4) {
            const uint32_t phase = elapsed % 1350;
            duty = phase < 600 && (phase / 100) % 2 == 0 ? brightness : 0;
        }
        if (duty != lastDuty) {
            ledcWrite(channel, duty);
            lastDuty = duty;
        }
    }

  private:
    uint8_t pin;
    uint8_t channel;
    std::atomic<uint16_t> command{255}; // Off until a display snapshot; full-brightness fault indication before that.
    uint32_t startedAt = 0;
    uint8_t previousMode = 0;
    uint8_t lastDuty = 0;
};
