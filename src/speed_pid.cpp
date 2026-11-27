#include "speed_pid.h"

#include "config.h"

#include <algorithm>
#include <cmath>

void speedPidReset(SpeedPidState& state) {
    state.integral = 0.0f;
    state.correction = 0.0f;
    state.lastMeasuredMmPerSecond = 0.0f;
    state.hasMeasurement = false;
    state.stalled = false;
    state.lastUpdateAt = 0;
    state.emptyWindows = 0;
}

float speedPidReferenceMmPerSecond(int commandedSpeed) {
    // Van toc do tham chieu ty le voi duty mo: duty 165/255 -> SPEED_PID_REFERENCE_MM_S.
    const float duty = std::fabs(static_cast<float>(commandedSpeed)) * 255.0f / MOTOR_SPEED_SCALE;
    const float magnitude = duty * SPEED_PID_REFERENCE_MM_S / 165.0f;
    return commandedSpeed < 0 ? -magnitude : magnitude;
}

static int applyCorrection(int commandedSpeed, float correction) {
    const float limit = std::fabs(static_cast<float>(commandedSpeed)) * SPEED_PID_OUT_LIMIT;
    const float bounded = std::min(std::max(correction, -limit), limit);
    const float adjusted = static_cast<float>(commandedSpeed) + bounded;
    const float clamped = std::min(std::max(adjusted, -static_cast<float>(MOTOR_SPEED_SCALE)),
                                  static_cast<float>(MOTOR_SPEED_SCALE));
    return static_cast<int>(std::lround(clamped));
}

int speedPidCorrect(SpeedPidState& state,
                    float targetMmPerSecond,
                    float measuredMmPerSecond,
                    int commandedSpeed,
                    unsigned long now) {
    // Chua den hanh PI (chua du chu ky) -> tra ve lenh goc.
    if (state.lastUpdateAt == 0) {
        state.lastUpdateAt = now;
        return commandedSpeed;
    }

    if (now - state.lastUpdateAt < SPEED_PID_PERIOD_MS) {
        return state.stalled ? -1 : applyCorrection(commandedSpeed, state.correction);
    }
    const unsigned long elapsed = now - state.lastUpdateAt;
    state.lastUpdateAt = now;

    // Khong co mau van toc do hop le (cu so xung chua du) -> giu nguyen lenh.
    if (!std::isfinite(measuredMmPerSecond)) {
        state.correction = 0.0f;
        return commandedSpeed;
    }

    const float previousMeasured = state.lastMeasuredMmPerSecond;
    const bool hadMeasurement = state.hasMeasurement;
    state.hasMeasurement = true;
    state.lastMeasuredMmPerSecond = measuredMmPerSecond;

    // Bao "mat encoder": cua so xung rong qua nhieu lan lien tiep trong khi
    // xe van duoc lenh chay -> bao loi, dung tich phan de tranh lang.
    if (std::fabs(measuredMmPerSecond) < 1.0f && commandedSpeed != 0) {
        ++state.emptyWindows;
        if (state.emptyWindows >= SPEED_PID_STALL_WINDOWS) {
            state.stalled = true;
        }
    } else {
        state.emptyWindows = 0;
        state.stalled = false;
    }
    if (state.stalled) {
        return -1;
    }

    const float error = targetMmPerSecond - measuredMmPerSecond;

    // Integral co gioi han + chong windup: dung tich phan khi output da bao ho.
    const float integralCandidate = state.integral + error * (elapsed / 1000.0f);
    const float clampedIntegral =
        std::min(std::max(integralCandidate, -SPEED_PID_I_LIMIT), SPEED_PID_I_LIMIT);

    // Derivative tren sai lech van toc do (mac dinh Kd = 0 vi do luoi).
    const float derivative = (hadMeasurement && SPEED_PID_KD != 0.0f)
                                 ? (measuredMmPerSecond - previousMeasured) / (elapsed / 1000.0f)
                                 : 0.0f;

    const float correction = SPEED_PID_KP * error
                           + SPEED_PID_KI * clampedIntegral
                           + SPEED_PID_KD * derivative;

    // Gioi han lech so theo ty le voi lenh goc (15%).
    const float limit = std::fabs(static_cast<float>(commandedSpeed)) * SPEED_PID_OUT_LIMIT;
    // Anti-windup: chi chap nhan integral moi khi output chua bao ho.
    const bool saturated = std::fabs(correction) > limit;
    if (!saturated) {
        state.integral = clampedIntegral;
    }

    state.correction = correction;
    return applyCorrection(commandedSpeed, state.correction);
}
