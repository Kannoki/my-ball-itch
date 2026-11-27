#pragma once

#include <stdint.h>

struct WheelOdometryGeometry {
    float wheelDiameterMm;
    float trackWidthMm;
    int pulsesPerRevolution;

    float distanceMm(float pulses) const;
    float yawDegrees(int32_t pulseDifference) const;
    int32_t turnPulses(float degrees) const;
};

float lineBearingDegrees(float leftOffsetMm, float sensorForwardMm);
