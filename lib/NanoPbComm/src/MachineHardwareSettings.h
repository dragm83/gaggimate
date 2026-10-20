#pragma once
#include "Messages.h"
#include <cmath>
#include <cstddef>
#include <cstring>
inline bool validMachineHardware(const gm::MachineHardwareSettings &v) {
    return v.version == 4 &&
           !(!std::isfinite(v.ntc_brew_nominal_resistance) || v.ntc_brew_nominal_resistance < 100 ||
             v.ntc_brew_nominal_resistance > 1000000 || !std::isfinite(v.ntc_brew_series_resistance) ||
             v.ntc_brew_series_resistance < 100 || v.ntc_brew_series_resistance > 1000000 ||
             !std::isfinite(v.ntc_brew_supply_voltage) || v.ntc_brew_supply_voltage < 0.5 || v.ntc_brew_supply_voltage > 5.5 ||
             !std::isfinite(v.ntc_brew_beta) || v.ntc_brew_beta < 1000 || v.ntc_brew_beta > 10000 ||
             !std::isfinite(v.ntc_steam_nominal_resistance) || v.ntc_steam_nominal_resistance < 100 ||
             v.ntc_steam_nominal_resistance > 1000000 || !std::isfinite(v.ntc_steam_series_resistance) ||
             v.ntc_steam_series_resistance < 100 || v.ntc_steam_series_resistance > 1000000 ||
             !std::isfinite(v.ntc_steam_supply_voltage) || v.ntc_steam_supply_voltage < 0.5 || v.ntc_steam_supply_voltage > 5.5 ||
             !std::isfinite(v.ntc_steam_beta) || v.ntc_steam_beta < 1000 || v.ntc_steam_beta > 10000 ||
             v.steam_level_debounce_checks < 1 || v.steam_level_debounce_checks > 100);
}

inline gm::MachineHardwareSettings defaultMachineHardware() {
    gm::MachineHardwareSettings v = gaggimate_MachineHardwareSettings_init_zero;
    v.version = 4;
    v.ntc_brew_nominal_resistance = v.ntc_steam_nominal_resistance = 50000;
    v.ntc_brew_series_resistance = v.ntc_steam_series_resistance = 10000;
    v.ntc_brew_supply_voltage = v.ntc_steam_supply_voltage = 4.096f;
    v.ntc_brew_beta = v.ntc_steam_beta = 3988;
    v.steam_level_enabled = true;
    v.system_led_enabled = true;
    v.steam_level_debounce_checks = 25;
    return v;
}
// Hash named fields, never struct padding or transient restart commands.
inline uint32_t machineHardwareHash(const gm::MachineHardwareSettings &v) {
    uint32_t hash = 2166136261u;
    auto add = [&](uint32_t n) {
        for (unsigned i = 0; i < 4; ++i) {
            hash = (hash ^ (n & 255)) * 16777619u;
            n >>= 8;
        }
    };
    auto number = [&](float n) {
        uint32_t bits;
        std::memcpy(&bits, &n, sizeof(bits));
        add(bits);
    };
    number(v.ntc_brew_nominal_resistance);
    number(v.ntc_brew_series_resistance);
    number(v.ntc_brew_supply_voltage);
    number(v.ntc_brew_beta);
    number(v.ntc_steam_nominal_resistance);
    number(v.ntc_steam_series_resistance);
    number(v.ntc_steam_supply_voltage);
    number(v.ntc_steam_beta);
    add(v.version);
    add(v.tank_level_enabled);
    add(v.steam_level_enabled);
    add(v.system_led_enabled);
    add(v.steam_level_debounce_checks);
    return hash;
}
inline bool decodeStoredMachineHardware(const void *bytes, size_t size, gm::MachineHardwareSettings &value) {
    if (size != sizeof(value))
        return false;
    std::memcpy(&value, bytes, size);
    return validMachineHardware(value);
}
