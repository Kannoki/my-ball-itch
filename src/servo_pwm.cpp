#include "servo_pwm.h"
#include <esp_arduino_version.h>

namespace {
constexpr uint32_t SERVO_PWM_FREQ_HZ = 50;
constexpr uint8_t SERVO_PWM_RESOLUTION_BITS = 14;
constexpr uint32_t SERVO_PWM_PERIOD_US = 20000UL;
constexpr uint32_t SERVO_PWM_MAX_TICKS = 1UL << SERVO_PWM_RESOLUTION_BITS;

uint32_t microsecondsToDuty(int us) {
    if (us < 0) us = 0;
    return (static_cast<uint32_t>(us) * SERVO_PWM_MAX_TICKS + (SERVO_PWM_PERIOD_US / 2)) / SERVO_PWM_PERIOD_US;
}
} // namespace

void servoPwmInit(int pin, int channel, int initialUs) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcAttachChannel(pin, SERVO_PWM_FREQ_HZ, SERVO_PWM_RESOLUTION_BITS, channel);
#else
    ledcSetup(channel, SERVO_PWM_FREQ_HZ, SERVO_PWM_RESOLUTION_BITS);
    ledcAttachPin(pin, channel);
#endif
    servoPwmWriteUs(pin, channel, initialUs);
}

void servoPwmWriteUs(int pin, int channel, int us) {
    const uint32_t duty = microsecondsToDuty(us);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    (void)channel;
    ledcWrite(pin, duty);
#else
    (void)pin;
    ledcWrite(channel, duty);
#endif
}
