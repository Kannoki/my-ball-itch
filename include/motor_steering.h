#pragma once

#include <stdint.h>

struct WheelSpeeds {
    int left;
    int right;
};

// Lech phai la duong, don vi theo buoc giua hai mat cam bien.
bool getLinePosition(uint8_t raw, float& position);

class MotorSteering {
public:
    WheelSpeeds update(uint8_t raw, int speed, unsigned long now);
    void reset();

private:
    float error = 0;
    unsigned long lastSteeredAt = 0;
    bool hasSample = false;
};
