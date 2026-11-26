#include "line_tracking.h"

namespace {
enum State { FOLLOW, TURN_LEFT, TURN_RIGHT, SEARCH, CROSS, STOPPED };
State state = FOLLOW;
unsigned long startedAt = 0;
uint8_t previousRaw = 0, olderRaw = 0;
unsigned sampleCount = 0;
float lastPosition = 0.0f;
float previousError = 0.0f;
bool hasError = false;
bool wasAllBlack = false;
int leftPwm = 0, rightPwm = 0;
int searchDirection = 0;

void drive(int left, int right) {
    leftPwm = left;
    rightPwm = right;
}

void enter(State next, unsigned long now) {
    state = next;
    startedAt = now;
    hasError = false;
}

// Tim tung cum lien tuc. Neu co nhieu cum, chon cum gan vi tri line truoc do.
// Khong lay trung binh hai nhanh thanh mot line ao nam giua khoang trang.
bool findCluster(uint8_t raw, float& position) {
    bool found = false;
    float bestDistance = 0.0f;
    for (int i = 0; i < 8; ++i) {
        if (!(raw & (1U << i))) continue;
        const int first = i;
        while (i + 1 < 8 && (raw & (1U << (i + 1)))) ++i;
        const float center = (first + i) * 0.5f - 3.5f;
        const float distance = fabsf(center - lastPosition);
        if (!found || distance < bestDistance) {
            found = true;
            bestDistance = distance;
            position = center;
        }
    }
    return found;
}

int curveDuty(int requested, int minimum) {
    if (requested < minimum * 3 / 4) return 0;
    return constrain(requested, minimum, MOTOR_MAX_SPEED_PWM);
}
} // namespace

void resetLineTracking() {
    state = FOLLOW;
    startedAt = 0;
    previousRaw = olderRaw = 0;
    sampleCount = 0;
    lastPosition = previousError = 0.0f;
    hasError = wasAllBlack = false;
    searchDirection = 0;
    drive(0, 0);
}

void updateLineTracking(uint8_t raw, unsigned long now, int baseSpeedPwm) {
    const bool allBlack = raw == 0xFF && wasAllBlack;
    wasAllBlack = raw == 0xFF;
    const uint8_t filtered = (raw & previousRaw) | (raw & olderRaw) | (previousRaw & olderRaw);
    olderRaw = previousRaw;
    previousRaw = raw;
    if (++sampleCount < 3) { drive(0, 0); return; }
    sampleCount = 3;

    // Dung do timeout chi duoc reset khi nguoi dung tat/bat lai auto (hoac dao logic).
    if (state == STOPPED) { drive(0, 0); return; }

    const int speed = constrain(baseSpeedPwm, MOTOR_MIN_SPEED_PWM, MOTOR_MAX_SPEED_PWM);
    const int turnSpeed = min(speed, MOTOR_SPIN_SPEED_PWM);
    const bool centered = (filtered & 0x18) != 0 && (filtered & 0xC3) == 0;

    if (state == TURN_LEFT || state == TURN_RIGHT) {
        if (now - startedAt >= LINE_TURN_TIMEOUT_MS) {
            enter(STOPPED, now);
            drive(0, 0);
        } else if (now - startedAt >= LINE_TURN_MIN_MS && centered) {
            enter(FOLLOW, now);
            lastPosition = 0;
            drive(speed, speed);
        } else if (state == TURN_LEFT) {
            drive(-turnSpeed, turnSpeed);
        } else {
            drive(turnSpeed, -turnSpeed);
        }
        return;
    }

    if (state == CROSS) {
        if (now - startedAt < CROSS_LINE_PASS_DELAY_MS) {
            drive(speed, speed);
            return;
        }
        if (filtered == 0xFF) {
            enter(STOPPED, now);
            drive(0, 0);
            return;
        }
        enter(FOLLOW, now);
    } else if (allBlack) {
        enter(CROSS, now);
        drive(speed, speed);
        return;
    }

    float position = 0.0f;
    if (!findCluster(filtered, position)) {
        if (state != SEARCH) {
            searchDirection = lastPosition < -0.25f ? -1 : lastPosition > 0.25f ? 1 : 0;
            enter(SEARCH, now);
        }
        if (searchDirection == 0 || now - startedAt >= LINE_SEARCH_TIMEOUT_MS) {
            enter(STOPPED, now);
            drive(0, 0);
        } else {
            const int searchSpeed = min(speed, MOTOR_SEARCH_SPEED_PWM);
            // Vi tri am = line ben trai: banh trai lui, banh phai tien.
            if (searchDirection < 0) drive(-searchSpeed, searchSpeed);
            else drive(searchSpeed, -searchSpeed);
        }
        return;
    }

    if (state == SEARCH) enter(FOLLOW, now);
    lastPosition = position;
    const bool leftCorner = (filtered & 0x01) && !(filtered & 0x80) && position <= -LINE_TURN_POSITION;
    const bool rightCorner = (filtered & 0x80) && !(filtered & 0x01) && position >= LINE_TURN_POSITION;
    if (leftCorner || rightCorner) {
        enter(leftCorner ? TURN_LEFT : TURN_RIGHT, now);
        if (leftCorner) drive(-turnSpeed, turnSpeed);
        else drive(turnSpeed, -turnSpeed);
        return;
    }

    // Dieu chinh theo trong tam cum: line sang phai -> banh trai nhanh hon.
    const float derivative = hasError ? position - previousError : 0.0f;
    previousError = position;
    hasError = true;
    const float correction = PID_KP * position + PID_KD * derivative;
    const int cruise = max(MOTOR_MIN_SPEED_PWM, speed - static_cast<int>(LINE_CURVE_SPEED_DROP * fabsf(position)));
    drive(curveDuty(static_cast<int>(cruise + correction), MOTOR_MIN_PWM_LEFT),
          curveDuty(static_cast<int>(cruise - correction), MOTOR_MIN_PWM_RIGHT));
}

int getLineLeftPwm() { return leftPwm; }
int getLineRightPwm() { return rightPwm; }

const char* getLineTrackingStatus() {
    switch (state) {
        case TURN_LEFT: return "TURN_LEFT";
        case TURN_RIGHT: return "TURN_RIGHT";
        case SEARCH: return "SEARCH";
        case CROSS: return "CROSS_PASS";
        case STOPPED: return "STOPPED";
        default: return "FOLLOW";
    }
}
