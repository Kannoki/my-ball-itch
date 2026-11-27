#include "wheel_odometry.h"

#include <cmath>

float WheelOdometryGeometry::distanceMm(float pulses) const {
    return pulses * (3.14159265f * wheelDiameterMm) / pulsesPerRevolution;
}

float WheelOdometryGeometry::yawDegrees(int32_t pulseDifference) const {
    // Delta yaw = (quang duong phai - trai) / be ngang hai banh.
    return pulseDifference * (180.0f * wheelDiameterMm) / (pulsesPerRevolution * trackWidthMm);
}

int32_t WheelOdometryGeometry::turnPulses(float degrees) const {
    return static_cast<int32_t>(std::lround(degrees * pulsesPerRevolution * trackWidthMm /
                                          (180.0f * wheelDiameterMm)));
}

float lineBearingDegrees(float leftOffsetMm, float sensorForwardMm) {
    return std::atan2(leftOffsetMm, sensorForwardMm) * (180.0f / 3.14159265f);
}
