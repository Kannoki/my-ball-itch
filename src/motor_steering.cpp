#include "motor_steering.h"
#include "config.h"

#include <algorithm>
#include <cmath>

void MotorSteering::reset() {
    hasSample = false;
}

bool getLinePosition(uint8_t raw, float& position) {
    int count = 0;
    int sum = 0;
    for (int i = 0; i < NUM_SENSORS; ++i) {
        if (raw & (1U << i)) {
            ++count;
            sum += 2 * i - (NUM_SENSORS - 1);
        }
    }
    if (count == 0 || count > (NUM_SENSORS + 1) / 2) {
        // Vach ngang rong khong cho vi tri lech tam dang tin cay.
        return false;
    }
    position = 0.5f * sum / count;
    return true;
}

// Giu banh trong khong bao gio dung that khi van con thay vach.
// Spec yeu cau "ca hai banh van tien" khi lech nhe sang mot ben.
// Khong san o day thi lenh banh trong xuong duoi MOTOR_DEADBAND_PWM_* va
// bi driver dung that -> xe khong cong duoc ma xoay vong.
static int floorInnerWheel(int wheelSpeed) {
    const int floorSpeed = MOTOR_MIN_SPEED;
    return wheelSpeed < floorSpeed ? floorSpeed : wheelSpeed;
}

WheelSpeeds MotorSteering::update(uint8_t raw, int speed, unsigned long now) {
    float position;
    if (!getLinePosition(raw, position)) {
        reset();
        return {speed, speed};
    }

    const unsigned long elapsed = now - lastSteeredAt;
    float derivative = 0;
    if (hasSample && elapsed && elapsed <= 3 * LINE_STEERING_REFERENCE_MS) {
        const float intervals = static_cast<float>(elapsed) / LINE_STEERING_REFERENCE_MS;
        const float alpha = 1.0f - std::pow(1.0f - LINE_ERROR_FILTER_ALPHA, intervals);
        const float change = alpha * (position - error);
        error += change;
        derivative = change / intervals;
    } else {
        error = position;
    }
    lastSteeredAt = now;
    hasSample = true;

    if (std::fabs(error) < 0.01f && std::fabs(derivative) < 0.01f) return {speed, speed};

    // He so PD cua demo dung don vi PWM, runLR nhan toc do chuan hoa.
    constexpr float scale = MOTOR_SPEED_SCALE / 255.0f;
    const float cruise = std::fmax(0.0f, speed - scale * LINE_CURVE_SPEED_DROP * std::fabs(error));
    const float correction = scale * (LINE_STEERING_KP * error + LINE_STEERING_KD * derivative);
    const auto limit = [](float command) {
        return static_cast<int>(std::fmin(static_cast<float>(MOTOR_MAX_SPEED), std::fmax(0.0f, command)));
    };

    int left = limit(cruise + correction);
    int right = limit(cruise - correction);

    // San banh trong: chi ap khi van con thay vach (raw khac 0).
    // Khi mat vach (raw == 0) ham nay khong chay, vi luc do xe dung hoac
    // xoay tim lai do xu ly o tang tren.
    if (raw != 0) {
        left = floorInnerWheel(left);
        right = floorInnerWheel(right);
    }

    return {left, right};
}
