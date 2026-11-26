#include "motor_steering.h"
#include "config.h"

#include <cmath>

// PD gains are in PWM units. The driver applies per-wheel deadbands and ramping.
WheelSpeeds MotorSteering::update(uint8_t raw, int speed, unsigned long now) {
    int count = 0;
    int sum = 0;
    for (int i = 0; i < NUM_SENSORS; ++i) {
        if (raw & (1U << i)) {
            ++count;
            sum += 2 * i - (NUM_SENSORS - 1);
        }
    }
    if (!count || count > (NUM_SENSORS + 1) / 2) {
        // Transverse marks give no reliable lateral position.
        lastSteeredAt = 0;
        return {speed, speed};
    }
    const float position = 0.5f * sum / count;
    const unsigned long dt = now - lastSteeredAt;
    float derivative = 0;
    if (lastSteeredAt && dt && dt <= 3 * LINE_SAMPLE_INTERVAL_MS) {
        const float change = LINE_ERROR_FILTER_ALPHA * (position - error);
        error += change;
        derivative = change * LINE_SAMPLE_INTERVAL_MS / dt;
    } else {
        error = position;
    }
    lastSteeredAt = now;
    if (fabsf(error) < 0.01f && fabsf(derivative) < 0.01f) return {speed, speed};
    const float scale = MOTOR_SPEED_SCALE / 255.0f;
    const float cruise = fmaxf(0, speed - scale * LINE_CURVE_SPEED_DROP * fabsf(error));
    const float correction = scale * (PID_KP * error + PID_KD * derivative);
    auto limit = [](float value) {
        return static_cast<int>(fminf(MOTOR_MAX_SPEED, fmaxf(0, value)));
    };
    return {limit(cruise + correction), limit(cruise - correction)};
}
