#include "line_tracking.h"
#include "line_sensor.h"
#include "motor_driver.h"
#include "servo2.h"
#include "servo3.h"

#include <atomic>
#include "motor_steering.h"

static std::atomic<uint32_t> crossReverseDuration{CROSS_REVERSE_DEFAULT_MS};

uint32_t getCrossReverseDuration() {
    return crossReverseDuration.load(std::memory_order_relaxed);
}

void setCrossReverseDuration(uint32_t durationMs) {
    crossReverseDuration.store(durationMs, std::memory_order_relaxed);
}

static void driveLine(LineAction action, int speed) {
    switch (action) {
        case LINE_LEFT:    runLR(0, speed, MOTOR_OUTPUT_PERCENT); break;
        case LINE_RIGHT:   runLR(speed, 0, MOTOR_OUTPUT_PERCENT); break;
        case LINE_FORWARD:
        case LINE_CROSS:   runLR(speed, speed, MOTOR_OUTPUT_PERCENT); break;
        case LINE_STOP:    stopMotion(); break;
    }
}

struct LineTrackingState {
    LineAction previousSample = LINE_STOP;
    unsigned long sampledAt = 0;
    uint8_t activeLevel = LINE_ACTIVE_STATE;
    bool enabled = false;
    MotorSteering steering;

    LineAction appliedAction = LINE_STOP;
    bool settlingServos = false;
    unsigned long servosStartedAt = 0;

    LineAction anchor = LINE_STOP;
    unsigned long anchorStartedAt = 0;
    int anchorSpeed = 0;
    bool followingStraight = false;
    unsigned long straightStartedAt = 0;
    int straightSpeed = 0;

    enum class Recovery { Follow, UndoStraight, UndoEntry, Retry, WaitForStraight };
    Recovery recovery = Recovery::Follow;
    unsigned long phaseStartedAt = 0;
    unsigned long entryDuration = 0;
    unsigned long straightDuration = 0;
    unsigned long retryElapsed = 0;
    LineAction retryAction = LINE_STOP;

    unsigned int bendsSeen = 0;
    bool turningCorner = false;
    unsigned long cornerStartedAt = 0;
    bool finishBar = false;
    bool finished = false;
    int entryLeft = 0, entryRight = 0;
    int straightLeft = 0, straightRight = 0;
    bool startupComplete = false;
    bool sequenceArmed = false;
    bool confirmingCrossClear = false;
    unsigned long crossClearStartedAt = 0;
    unsigned int completedSequences = 0;

    bool awaitingCrossForward = false;
    bool confirmingCrossForward = false;
    unsigned long crossForwardStartedAt = 0;

    unsigned int sequenceStep = 0;
    unsigned long sequenceStartedAt = 0;
    unsigned long sequenceReverseMs = 0;
    int sequenceSpeed = 0;
};

static LineTrackingState tracking;

using Recovery = LineTrackingState::Recovery;

static void homeCrossServos(unsigned long now) {
    constexpr int homeAngle = 180;
    tracking.settlingServos = servo2_getAngle() != homeAngle || servo3_getAngle() != homeAngle;
    tracking.servosStartedAt = now;
    if (servo2_getAngle() != homeAngle) servo2_setAngle(homeAngle);
    if (servo3_getAngle() != homeAngle) servo3_setAngle(homeAngle);
}

struct CrossStep {
    int leftDirection;
    int rightDirection;
    unsigned long durationMs;
    void (*setServoAngle)(int);
    int angle;
};

static constexpr CrossStep crossSteps[] = {
    { 1, -1, 2 * CROSS_RIGHT_45_MS, nullptr, 0},
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0},
    {-1, -1, 0, nullptr, 0}, // Thoi gian lui duoc chot khi bat dau chuoi.
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0},
    { 0,  0, CROSS_SERVO_SETTLE_MS, servo2_setAngle, 0},
    { 0,  0, CROSS_SERVO_SETTLE_MS, servo3_setAngle, 45},
    { 1, -1, CROSS_RIGHT_45_MS, nullptr, 0},
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0},
    { 0,  0, CROSS_SERVO_SETTLE_MS, servo3_setAngle, 180},
    { 0,  0, CROSS_SERVO_SETTLE_MS, servo2_setAngle, 180},
    {-1,  1, CROSS_LEFT_45_MS, nullptr, 0},
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0},
    { 1,  1, 0, nullptr, 0}, // Di tien lai cung thoi gian vua lui.
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0},
    {-1,  1, 2 * CROSS_LEFT_45_MS, nullptr, 0},
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0},
};
static constexpr unsigned int crossStepCount = sizeof(crossSteps) / sizeof(crossSteps[0]);

static void startCrossStep(unsigned long now) {
    const CrossStep& step = crossSteps[tracking.sequenceStep - 1];
    tracking.sequenceStartedAt = now;
    if (step.setServoAngle) step.setServoAngle(step.angle);
    const int speed = step.leftDirection != step.rightDirection ? MOTOR_PIVOT_SPEED : tracking.sequenceSpeed;
    runLR(step.leftDirection * speed, step.rightDirection * speed,
          MOTOR_OUTPUT_PERCENT);
}

static void updateCrossSequence(unsigned long now) {
    const CrossStep& step = crossSteps[tracking.sequenceStep - 1];
    const unsigned long duration = step.durationMs > 0 ? step.durationMs : tracking.sequenceReverseMs;
    if (now - tracking.sequenceStartedAt < duration) return;

    if (tracking.sequenceStep == crossStepCount) {
        tracking.sequenceStep = 0;
        ++tracking.completedSequences;
        tracking.sampledAt = now;
        tracking.previousSample = LINE_STOP;
        return;
    }
    ++tracking.sequenceStep;
    startCrossStep(now);
}

static void clearAnchor() {
    tracking.anchor = LINE_STOP;
    tracking.followingStraight = false;
}

static void startUndoEntry(unsigned long now) {
    tracking.recovery = Recovery::UndoEntry;
    tracking.phaseStartedAt = now;
    runLR(-tracking.entryLeft, -tracking.entryRight, MOTOR_OUTPUT_PERCENT);
}

static void startRecovery(unsigned long now) {
    tracking.entryDuration = (tracking.followingStraight ? tracking.straightStartedAt : now)
                             - tracking.anchorStartedAt;
    if (!tracking.followingStraight) {
        startUndoEntry(now);
        return;
    }
    tracking.straightDuration = now - tracking.straightStartedAt;
    tracking.recovery = Recovery::UndoStraight;
    tracking.phaseStartedAt = now;
    runLR(-tracking.straightLeft, -tracking.straightRight, MOTOR_OUTPUT_PERCENT);
}

static void followLine(uint8_t raw, LineAction action, unsigned long now) {
    // Vach xuat phat chi dua xe di thang, chua phai moc lui hay tram servo.
    if (!tracking.startupComplete && action == LINE_CROSS) {
        clearAnchor();
        tracking.appliedAction = action;
        driveLine(action, motorBaseSpeed);
        return;
    }

    int speed = motorBaseSpeed;
    if (action == LINE_LEFT || action == LINE_RIGHT || action == LINE_CROSS) {
        if (tracking.appliedAction != action) {
            tracking.anchor = action;
            tracking.anchorStartedAt = now;
            tracking.anchorSpeed = speed;
            tracking.followingStraight = false;
        }
        speed = tracking.anchorSpeed;
    } else if (tracking.anchor != LINE_STOP) {
        if (!tracking.followingStraight) {
            tracking.followingStraight = true;
            tracking.straightStartedAt = now;
            tracking.straightSpeed = speed;
        }
        if (now - tracking.straightStartedAt >= LINE_RECOVERY_FORWARD_MS) {
            clearAnchor();
        } else {
            speed = tracking.straightSpeed;
        }
    }
    tracking.appliedAction = action;
    const WheelSpeeds wheels = tracking.steering.update(raw, speed, now);
    if (tracking.anchor != LINE_STOP) {
        if (tracking.followingStraight) {
            tracking.straightLeft = wheels.left;
            tracking.straightRight = wheels.right;
        } else {
            tracking.entryLeft = wheels.left;
            tracking.entryRight = wheels.right;
        }
    }
    runLR(wheels.left, wheels.right, MOTOR_OUTPUT_PERCENT);
}

static void updateRecovery(uint8_t raw, LineAction action, bool confirmed, unsigned long now, unsigned long elapsed) {
    switch (tracking.recovery) {
        case Recovery::UndoStraight:
            if (now - tracking.phaseStartedAt >= tracking.straightDuration) startUndoEntry(now);
            return;
        case Recovery::UndoEntry:
            if (now - tracking.phaseStartedAt < tracking.entryDuration) return;
            clearAnchor();
            tracking.appliedAction = LINE_STOP;
            tracking.recovery = Recovery::Retry;
            tracking.retryElapsed = 0;
            tracking.retryAction = confirmed ? action : LINE_STOP;
            driveLine(LINE_FORWARD, motorBaseSpeed);
            return;
        case Recovery::Retry:
            if (tracking.retryAction != LINE_CROSS) tracking.retryElapsed += elapsed;
            if (confirmed) tracking.retryAction = action;
            if (tracking.retryElapsed < LINE_RECOVERY_FORWARD_MS || tracking.retryAction == LINE_CROSS) return;
            if (!(confirmed && action == LINE_FORWARD)) {
                stopMotion();
                tracking.recovery = Recovery::WaitForStraight;
                return;
            }
            break;
        case Recovery::WaitForStraight:
            if (!confirmed || (action != LINE_FORWARD && action != LINE_CROSS)) return;
            break;
        case Recovery::Follow:
            return;
    }
    tracking.recovery = Recovery::Follow;
    followLine(raw, action, now);
}

static void trackEightSensors(uint8_t raw, LineAction action, bool confirmed, unsigned long now, unsigned long elapsed) {
    if (tracking.recovery != Recovery::Follow) {
        tracking.confirmingCrossClear = false;
        updateRecovery(raw, action, confirmed, now, elapsed);
        return;
    }

    if (tracking.finished) return;
    // A right-angle corner may sweep past the array before the axle turns.
    // Keep turning across a short gap; resume PD when the line reappears.
    if (confirmed && !tracking.turningCorner && action == LINE_RIGHT &&
        (raw == 0xF8 || raw == 0xFC)) {
        tracking.turningCorner = true;
        tracking.cornerStartedAt = now;
        tracking.awaitingCrossForward = false;
        tracking.confirmingCrossForward = false;
        tracking.confirmingCrossClear = false;
        tracking.steering = {};
        clearAnchor();
        tracking.appliedAction = LINE_STOP;
    }
    if (tracking.turningCorner) {
        if (confirmed && (action == LINE_FORWARD || action == LINE_CROSS || action == LINE_LEFT)) {
            tracking.turningCorner = false;
        } else {
            if (now - tracking.cornerStartedAt < 2 * CROSS_RIGHT_45_MS) {
                runLR(MOTOR_PIVOT_SPEED, -MOTOR_PIVOT_SPEED, MOTOR_OUTPUT_PERCENT);
            } else {
                stopMotion();
            }
            return;
        }
    }
    if (tracking.completedSequences == CROSS_SEQUENCE_LIMIT && confirmed) {
        if (tracking.finishBar && action == LINE_STOP) {
            tracking.finished = true;
            stopMotion();
            return;
        }
        tracking.finishBar = action == LINE_CROSS;
    }
    // The first spur precedes the S-bend. Wait for sustained steering
    // in both directions before accepting stations on the return straight.
    if (confirmed && tracking.startupComplete && action == tracking.anchor &&
        (action == LINE_LEFT || action == LINE_RIGHT) &&
        now - tracking.anchorStartedAt >= COURSE_BEND_CONFIRM_MS) {
        tracking.bendsSeen |= action == LINE_LEFT ? 1U : 2U;
    }
    if (action != LINE_FORWARD) {
        tracking.confirmingCrossForward = false;
        tracking.confirmingCrossClear = false;
    }
    // shortcut: nhan tram qua mau line; can ma moc rieng neu san co giao lo den khac.
    if (!tracking.sequenceArmed && tracking.completedSequences < CROSS_SEQUENCE_LIMIT &&
        confirmed && action == LINE_FORWARD) {
        if (!tracking.confirmingCrossClear) {
            tracking.confirmingCrossClear = true;
            tracking.crossClearStartedAt = now;
        } else if (now - tracking.crossClearStartedAt >= CROSS_FORWARD_CONFIRM_MS) {
            tracking.startupComplete = true;
            tracking.sequenceArmed = true;
            tracking.confirmingCrossClear = false;
        }
    }
    if (action == LINE_STOP && tracking.anchor == LINE_STOP) {
        tracking.awaitingCrossForward = false;
        tracking.appliedAction = LINE_STOP;
        stopMotion();
        return;
    }
    if (!confirmed) return;

    if (action == LINE_STOP) {
        tracking.awaitingCrossForward = false;
        startRecovery(now);
        return;
    }
    if (tracking.bendsSeen == 3 && tracking.sequenceArmed && action == LINE_CROSS && tracking.appliedAction == LINE_FORWARD) {
        tracking.awaitingCrossForward = true;
    } else if (action == LINE_LEFT || action == LINE_RIGHT) {
        tracking.awaitingCrossForward = false;
    }

    if (action == LINE_FORWARD && tracking.awaitingCrossForward) {
        if (!tracking.confirmingCrossForward) {
            tracking.confirmingCrossForward = true;
            tracking.crossForwardStartedAt = now;
        } else if (now - tracking.crossForwardStartedAt >= CROSS_FORWARD_CONFIRM_MS) {
            clearAnchor();
            tracking.appliedAction = LINE_STOP;
            tracking.previousSample = LINE_STOP;
            tracking.awaitingCrossForward = false;
            tracking.confirmingCrossForward = false;
            tracking.sequenceArmed = false;
            tracking.confirmingCrossClear = false;
            tracking.sequenceStep = 1;
            tracking.sequenceSpeed = motorBaseSpeed;
            tracking.sequenceReverseMs = getCrossReverseDuration();
            startCrossStep(now);
            return;
        }
    }
    followLine(raw, action, now);
}

void updateLineTracking(bool enabled) {
    if (!enabled) {
        if (tracking.enabled) {
            stopMotion();
            tracking = {};
        }
        return;
    }

    const unsigned long now = millis();
    if (!tracking.enabled || tracking.activeLevel != lineActiveState) {
        stopMotion();
        tracking = {};
        tracking.enabled = true;
        tracking.activeLevel = lineActiveState;
        tracking.sampledAt = now;
        homeCrossServos(now);
        return;
    }

    if (tracking.settlingServos) {
        if (now - tracking.servosStartedAt < CROSS_SERVO_SETTLE_MS) return;
        tracking.settlingServos = false;
    }
    if (tracking.sequenceStep != 0) {
        updateCrossSequence(now);
        return;
    }

    const unsigned long elapsed = now - tracking.sampledAt;
    if (elapsed < LINE_SAMPLE_INTERVAL_MS) return;
    tracking.sampledAt = now;
    const uint8_t raw = readRawSensors();
    const LineAction action = getLineAction(raw);
    const bool confirmed = action == tracking.previousSample;
    tracking.previousSample = action;
    trackEightSensors(raw, action, confirmed, now, elapsed);
}
