#ifndef TEMPERATURESENSOR_H
#define TEMPERATURESENSOR_H

#include <functional>

constexpr double MAX_SAFE_TEMP = 170.0;
using temperature_error_callback_t = std::function<void(const char *reason, float temperature)>;

class TemperatureSensor {
  public:
    virtual ~TemperatureSensor() = default;
    virtual float read();
    virtual bool isErrorState();
    virtual void setup();
};

#endif // TEMPERATURESENSOR_H
