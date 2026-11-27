#pragma once

#include "wheel_odometry.h"

struct WheelEncoderCounts {
    int32_t left;
    int32_t right;

    int32_t difference() const { return right - left; }
};

// Van toc do uoc luong theo so xung dem duoc trong mot cua so thoi gian.
struct WheelEncoderVelocity {
    float leftMmPerSecond;
    float rightMmPerSecond;
    bool  valid;   // false khi cua so qua rong (co the mat encoder)
};

void wheelEncodersInit();
bool wheelEncodersReady();
void resetWheelEncoderCounts();
void setWheelEncoderDirections(int left, int right);
WheelEncoderCounts getWheelEncoderCounts();
const WheelOdometryGeometry& getWheelOdometryGeometry();

// Do van toc do hai banh theo cua so thoi gian (SPEED_PID_WINDOW_MS).
// Van toc do duoc chuan hoa boi ENCODER_PULSE_SCALE_LEFT/RIGHT nen YAW va
// quang trong goc quay deu dung khi hai banh truot khac nhau.
WheelEncoderVelocity getWheelEncoderVelocity(unsigned long now);

// Sai so xung canh chinh (do luot) cho tung banh.
float getEncoderPulseScaleLeft();
float getEncoderPulseScaleRight();
