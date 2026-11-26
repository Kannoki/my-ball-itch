#pragma once

#include <stdint.h>

struct WheelSpeeds {
    int left;
    int right;
};

class MotorSteering {
public:
    WheelSpeeds update(uint8_t raw, int speed, unsigned long now);

private:
    float error = 0;
    unsigned long lastSteeredAt = 0;
};
