#include "line_tracking.h"
#include "line_sensor.h"
#include "motor_driver.h"
#include "motor_steering.h"
#include "servo2.h"
#include "servo3.h"
#if NUM_SENSORS == 8
#include "junction_detect.h"
#endif
#ifdef SENSOR_PHOTORESISTOR
#include "wheel_encoders.h"
#include "speed_pid.h"
#include <algorithm>
#include <cmath>
#endif

#include <atomic>

static std::atomic<uint32_t> crossReverseDuration{CROSS_REVERSE_DEFAULT_MS};

uint32_t getCrossReverseDuration() {
    return crossReverseDuration.load(std::memory_order_relaxed);
}

void setCrossReverseDuration(uint32_t durationMs) {
    crossReverseDuration.store(durationMs, std::memory_order_relaxed);
}

#ifdef SENSOR_PHOTORESISTOR
// ------------------------------------------------------------------------------
// VONG TRONG: SPEED PI TUNG BANH
// ------------------------------------------------------------------------------
static SpeedPidState speedPidLeft;
static SpeedPidState speedPidRight;

// Giu lenh van toc do trong pham vi cho phep.
static int clampSpeed(int speed) {
    if (speed > MOTOR_MAX_SPEED) return MOTOR_MAX_SPEED;
    if (speed < -MOTOR_MAX_SPEED) return -MOTOR_MAX_SPEED;
    return speed;
}

// Feed-forward tinh chiinh: giu hai ben thang khi dong co khac nhau.
static int applySpeedTrim(int speed, int trim) {
    return clampSpeed(speed + trim);
}
#endif

static void driveAutoWheels(int left, int right) {
    const int power = left == right ? MOTOR_OUTPUT_PERCENT : MOTOR_TURN_OUTPUT_PERCENT;
    runLR(left, right, power);
}

#ifdef SENSOR_PHOTORESISTOR
// Quay tai cho o giao lo dung truc tiep duty da chon, khong nhan he so do line.
static void drivePivot(int leftCommand, int rightCommand, int speed) {
    runLR(leftCommand * speed, rightCommand * speed, 100);
}

// Di chuyen theo van toc do co ban voi vong trong Speed PI dung vao.
// Neu encoder khong dung hoac mat tin hieu -> giu lenh mo va dung tich phan.
static void driveFollowing(int left, int right, unsigned long now) {
    if (!wheelEncodersReady()) {
        driveAutoWheels(left, right);
        return;
    }

    const WheelEncoderVelocity velocity = getWheelEncoderVelocity(now);
    if (!velocity.valid) {
        driveAutoWheels(left, right);
        return;
    }

    const int leftTrimmed = applySpeedTrim(left, SPEED_TRIM_LEFT);
    const int rightTrimmed = applySpeedTrim(right, SPEED_TRIM_RIGHT);

    const float leftTarget = speedPidReferenceMmPerSecond(leftTrimmed);
    const float rightTarget = speedPidReferenceMmPerSecond(rightTrimmed);

    int leftCorrected = speedPidCorrect(speedPidLeft, leftTarget, velocity.leftMmPerSecond,
                                        leftTrimmed, now);
    int rightCorrected = speedPidCorrect(speedPidRight, rightTarget, velocity.rightMmPerSecond,
                                         rightTrimmed, now);

    if (leftCorrected < 0 || rightCorrected < 0) {
        // Mat tin hieu encoder: bao loi, dung tich phan de tranh lang,
        // van chay theo lenh mo (khong dung hanh robot).
        Serial.println("[SpeedPI] Mat tin hieu encoder, dung tich phan");
        speedPidReset(speedPidLeft);
        speedPidReset(speedPidRight);
        leftCorrected = leftTrimmed;
        rightCorrected = rightTrimmed;
    }

    driveAutoWheels(leftCorrected, rightCorrected);
}
#endif

static void driveLine(LineAction action, int speed) {
    switch (action) {
        case LINE_LEFT:    driveAutoWheels(0, speed); break;
        case LINE_RIGHT:   driveAutoWheels(speed, 0); break;
        case LINE_FORWARD:
        case LINE_CROSS:   driveAutoWheels(speed, speed); break;
        case LINE_STOP:    stopMotion(); break;
    }
}

struct LineTrackingState {
    LineAction previousSample = LINE_STOP;
    unsigned long sampledAt = 0;
    uint8_t activeLevel = LINE_ACTIVE_STATE;
    bool enabled = false;

#if NUM_SENSORS == 5
    bool crossing = false;
    unsigned long crossStartedAt = 0;
#else
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
    WheelSpeeds entryWheels{};
    WheelSpeeds straightWheels{};
    enum class Recovery { Follow, UndoStraight, UndoEntry, Retry, WaitForStraight };
    Recovery recovery = Recovery::Follow;
    unsigned long phaseStartedAt = 0;
    unsigned long entryDuration = 0;
    unsigned long straightDuration = 0;
    unsigned long retryElapsed = 0;
    LineAction retryAction = LINE_STOP;

#ifdef SENSOR_PHOTORESISTOR
    // --- CHUONG TRINH DUONG CHAY ---
    // Giao lo (+) va tram servo deu la mot dai vach ngang cat qua thanh cam bien
    // -> KHONG phan biet duoc bang hinh hoc, chi phan biet duoc bang THU TU CHAY
    // (xem COURSE_STEP_* trong config.h):
    //   0: cho nga 3 (chu T)  -> quay TRAI 90 do
    //   1: cho giao lo (+)    -> quay PHAI 90 do
    //   2: 3 tram servo       -> chuoi thao tac servo
    //   3: het chuong trinh   -> chi di thang
    int courseStep = COURSE_STEP_WAIT_TEE;
    unsigned long startupCenterSeenAt = 0;
    enum class JunctionPhase { Waiting, Approaching, CenterSettling, Pivoting, Reacquiring };
    JunctionPhase junctionPhase = JunctionPhase::Waiting;
    int junctionConfirmCount = 0;
    int junctionReacquireCount = 0;
    WheelEncoderCounts junctionApproachStart{};
    unsigned long junctionPhaseStartedAt = 0;
    int junctionTrailingSamples = 0;
    float junctionTrailingMm = 0.0f;
    float junctionTargetMm = 0.0f;
    int junctionScanDirection = 0;
    bool junctionMarkerMeasured = false;
    unsigned long junctionFailedAt = 0;
    bool junctionCoolingDown = false;
#else
    enum class BranchPhase { Waiting, Turning, Following, Returning, Complete };
    BranchPhase branchPhase = BranchPhase::Waiting;
    bool previousBranchCandidate = false;
    bool branchLeftMainLine = false;
    bool previousBranchCenter = false;
    unsigned long branchTurnStartedAt = 0;
    struct BranchMotion {
        int left = 0;
        int right = 0;
        unsigned long duration = 0;
    };
    BranchMotion branchHistory[128];
    unsigned int branchMotionCount = 0;
    unsigned long branchMotionStartedAt = 0;
#endif

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
#ifdef SENSOR_PHOTORESISTOR
    WheelEncoderCounts sequenceTurnStart{};
    bool sequenceTurnSlowing = false;
    bool sequenceTurnFailed = false;

    // Tim lai duong khi mat vach: xoay tai cho sang phia vach vua mat.
    bool searchingLine = false;
    int lastLineDirection = -1; // Mac dinh tim sang trai neu chua thay vach.
    int lostLineSamples = 0;    // So mau lien tiep khong thay vach.
    int lineFoundSamples = 0;   // So mau lien tiep thay vach (de xac nhan lai).

    // Trang thai quay tai cho bang encoder (dung cho nga re 90 do).
    struct PivotState {
        int targetDegrees = 0;
        int direction = 0;          // +1 = quay trai, -1 = quay phai
        int32_t startLeft = 0;
        int32_t startRight = 0;
        bool slowing = false;
        unsigned long startedAt = 0;
    };
    PivotState pivot;
#endif
#endif
};

static LineTrackingState tracking;

#if NUM_SENSORS == 5
static void trackFiveSensors(LineAction action, bool confirmed, unsigned long now) {
    if (tracking.crossing && now - tracking.crossStartedAt >= CROSS_LINE_PASS_DELAY_MS) {
        stopMotion();
    }
    if (action == LINE_STOP) {
        tracking.crossing = false;
        stopMotion();
        return;
    }
    if (!confirmed) return;

    if (action == LINE_CROSS && !tracking.crossing) {
        tracking.crossStartedAt = now;
    }
    tracking.crossing = action == LINE_CROSS;
    if (!tracking.crossing || now - tracking.crossStartedAt < CROSS_LINE_PASS_DELAY_MS) {
        driveLine(action, motorBaseSpeed);
    }
}
#else
using Recovery = LineTrackingState::Recovery;
#ifdef SENSOR_PHOTORESISTOR
using JunctionPhase = LineTrackingState::JunctionPhase;
#else
using BranchPhase = LineTrackingState::BranchPhase;
#endif

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
    int turnDegrees;
};

static constexpr CrossStep crossSteps[] = {
    { 1, -1, 2 * CROSS_TURN_45_MS, nullptr, 0, -90},
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0, 0},
    {-1, -1, 0, nullptr, 0, 0}, // Thoi gian lui duoc chot khi bat dau chuoi.
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0, 0},
    { 0,  0, CROSS_SERVO_SETTLE_MS, servo2_setAngle, 0, 0},
    { 0,  0, CROSS_SERVO_SETTLE_MS, servo3_setAngle, 45, 0},
    { 1, -1, CROSS_TURN_45_MS, nullptr, 0, -45},
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0, 0},
    { 0,  0, CROSS_SERVO_SETTLE_MS, servo3_setAngle, 180, 0},
    { 0,  0, CROSS_SERVO_SETTLE_MS, servo2_setAngle, 180, 0},
    {-1,  1, CROSS_TURN_45_MS, nullptr, 0, 45},
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0, 0},
    { 1,  1, 0, nullptr, 0, 0}, // Di tien lai cung thoi gian vua lui.
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0, 0},
    {-1,  1, 2 * CROSS_TURN_45_MS, nullptr, 0, 90},
    { 0,  0, MOTOR_RAMP_MS, nullptr, 0, 0},
};
static constexpr unsigned int crossStepCount = sizeof(crossSteps) / sizeof(crossSteps[0]);

static void startCrossStep(unsigned long now) {
    const CrossStep& step = crossSteps[tracking.sequenceStep - 1];
    tracking.sequenceStartedAt = now;
    if (step.setServoAngle) step.setServoAngle(step.angle);
#ifdef SENSOR_PHOTORESISTOR
    if (step.turnDegrees && wheelEncodersReady()) {
        tracking.sequenceTurnStart = getWheelEncoderCounts();
        tracking.sequenceTurnSlowing = false;
        runLR(step.leftDirection * MOTOR_ENCODER_TURN_SPEED, step.rightDirection * MOTOR_ENCODER_TURN_SPEED);
        return;
    }
#endif
    driveAutoWheels(step.leftDirection * tracking.sequenceSpeed, step.rightDirection * tracking.sequenceSpeed);
}

#ifdef SENSOR_PHOTORESISTOR
static bool finishEncoderTurn(const CrossStep& step, unsigned long now) {
    const WheelEncoderCounts counts = getWheelEncoderCounts();
    const int32_t leftDelta = counts.left - tracking.sequenceTurnStart.left;
    const int32_t rightDelta = counts.right - tracking.sequenceTurnStart.right;
    const int32_t difference = rightDelta - leftDelta;
    const auto& geometry = getWheelOdometryGeometry();
    const int direction = step.turnDegrees > 0 ? 1 : -1;
    const int32_t remaining = geometry.turnPulses(direction * step.turnDegrees) - direction * difference;
    const bool bothWheelsMoved = leftDelta * step.leftDirection > 0 && rightDelta * step.rightDirection > 0;

    if (remaining <= 0 && bothWheelsMoved) {
        Serial.printf("[Turn] TARGET=%d deg TURN=%+.1f deg YAW=%+.1f deg\n", step.turnDegrees,
                      geometry.yawDegrees(difference), geometry.yawDegrees(counts.difference()));
        return true;
    }
    if (remaining <= 0 || now - tracking.sequenceStartedAt >= ENCODER_TURN_TIMEOUT_MS) {
        stopMotion();
        tracking.sequenceTurnFailed = true;
        Serial.printf("[Turn] Dung: TARGET=%d deg TURN=%+.1f deg | L=%ld R=%ld xung\n",
                      step.turnDegrees, geometry.yawDegrees(difference),
                      static_cast<long>(leftDelta), static_cast<long>(rightDelta));
        return false;
    }
    if (!tracking.sequenceTurnSlowing && remaining <= geometry.turnPulses(ENCODER_TURN_SLOW_DEG)) {
        tracking.sequenceTurnSlowing = true;
        driveAutoWheels(step.leftDirection * MOTOR_MIN_SPEED, step.rightDirection * MOTOR_MIN_SPEED);
    }
    return false;
}

// ------------------------------------------------------------------------------
// QUAY TAI CHO BANG ENCODER
// ------------------------------------------------------------------------------
// targetDegrees > 0 = quay trai, < 0 = quay phai.
// Quay trai: banh trai lui (-), banh phai tien (+). Chieu lech phai - trai > 0
// -> yaw duong, dung quy uoc "quay trai = duong".
static void startEncoderPivot(int targetDegrees, unsigned long now) {
    tracking.pivot.targetDegrees = targetDegrees;
    tracking.pivot.direction = targetDegrees >= 0 ? 1 : -1;
    tracking.pivot.slowing = false;
    tracking.pivot.startedAt = now;
    const WheelEncoderCounts counts = getWheelEncoderCounts();
    tracking.pivot.startLeft = counts.left;
    tracking.pivot.startRight = counts.right;
    drivePivot(-tracking.pivot.direction, tracking.pivot.direction, MOTOR_MAX_SPEED);
}

enum class PivotResult { Turning, Reached, Failed };

static PivotResult updateEncoderPivot(unsigned long now) {
    const auto& geometry = getWheelOdometryGeometry();
    const WheelEncoderCounts counts = getWheelEncoderCounts();
    const int32_t leftDelta = counts.left - tracking.pivot.startLeft;
    const int32_t rightDelta = counts.right - tracking.pivot.startRight;
    const int32_t difference = rightDelta - leftDelta;

    const float targetMagnitude = tracking.pivot.targetDegrees >= 0
                                      ? static_cast<float>(tracking.pivot.targetDegrees)
                                      : static_cast<float>(-tracking.pivot.targetDegrees);
    const int32_t targetPulses = geometry.turnPulses(targetMagnitude);
    const int32_t remaining = targetPulses - tracking.pivot.direction * difference;

    const int leftCommand = -tracking.pivot.direction;
    const int rightCommand = tracking.pivot.direction;
    const bool bothWheelsMoved = leftDelta * leftCommand > 0 && rightDelta * rightCommand > 0;

    if (remaining <= 0 && bothWheelsMoved) {
        coastMotionAtJunction();
        Serial.printf("[Pivot] TARGET=%+d deg TURN=%+.1f deg YAW=%+.1f deg MS=%lu\n",
                      tracking.pivot.targetDegrees, geometry.yawDegrees(difference),
                      geometry.yawDegrees(counts.difference()), now - tracking.pivot.startedAt);
        return PivotResult::Reached;
    }
    if (remaining <= 0 || now - tracking.pivot.startedAt >= JUNCTION_PIVOT_TIMEOUT_MS) {
        stopMotion();
        Serial.printf("[Pivot] Dung: TARGET=%+d deg TURN=%+.1f deg | L=%ld R=%ld xung\n",
                      tracking.pivot.targetDegrees, geometry.yawDegrees(difference),
                      static_cast<long>(leftDelta), static_cast<long>(rightDelta));
        return PivotResult::Failed;
    }
    // Gan muc tien -> giam toc do de quay chinh xac.
    if (!tracking.pivot.slowing && remaining <= geometry.turnPulses(JUNCTION_TURN_SLOW_DEG)) {
        tracking.pivot.slowing = true;
        drivePivot(leftCommand, rightCommand, JUNCTION_FINE_TURN_SPEED);
    }
    return PivotResult::Turning;
}
#endif

static void updateCrossSequence(unsigned long now) {
    const CrossStep& step = crossSteps[tracking.sequenceStep - 1];
    const unsigned long duration = step.durationMs > 0 ? step.durationMs : tracking.sequenceReverseMs;
#ifdef SENSOR_PHOTORESISTOR
    if (step.turnDegrees && wheelEncodersReady()) {
        if (!finishEncoderTurn(step, now)) return;
    } else
#endif
    if (now - tracking.sequenceStartedAt < duration) {
        return;
    }

    if (tracking.sequenceStep == crossStepCount) {
        tracking.sequenceStep = 0;
        ++tracking.completedSequences;
#ifdef SENSOR_PHOTORESISTOR
        if (tracking.completedSequences >= CROSS_SEQUENCE_LIMIT) tracking.courseStep = COURSE_STEP_DONE;
#endif
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

#ifdef SENSOR_PHOTORESISTOR
static void searchForLine();

static void failJunction(unsigned long now, const char* reason) {
    coastMotionAtJunction();
    tracking.junctionPhase = JunctionPhase::Waiting;
    tracking.junctionConfirmCount = 0;
    tracking.junctionCoolingDown = true;
    tracking.junctionFailedAt = now;
    tracking.searchingLine = false;
    tracking.lostLineSamples = 0;
    tracking.lineFoundSamples = 0;
    Serial.printf("[Junction] FAIL=%s COURSE=%d, quay lai do line\n", reason, tracking.courseStep);
}

static void measureJunctionMarker(uint8_t raw, float traveled) {
    if (tracking.junctionMarkerMeasured) return;
    if (detectJunction(raw) != JUNCTION_NONE) {
        tracking.junctionTrailingSamples = 0;
    } else if (++tracking.junctionTrailingSamples == 1) {
        tracking.junctionTrailingMm = traveled;
    }

    const float maxMarkerWidth = 3.0f * LINE_WIDTH_MM;
    const bool hasTrailing = tracking.junctionTrailingSamples >= JUNCTION_CONFIRM_SAMPLES;
    if (!hasTrailing && traveled < maxMarkerWidth) return;

    const bool hasSpan = hasTrailing && tracking.junctionTrailingMm > 0.0f &&
                         tracking.junctionTrailingMm <= maxMarkerWidth;
    const float width = hasSpan ? tracking.junctionTrailingMm : LINE_WIDTH_MM;
    tracking.junctionMarkerMeasured = true;
    tracking.junctionTargetMm = LINE_SENSOR_FORWARD_MM + 0.5f * width;
    Serial.printf("[Junction] MARK=%s WIDTH=%.1f mm TARGET=%.1f mm\n",
                  hasSpan ? "measured" : "nominal", width, tracking.junctionTargetMm);
}

static float junctionApproachDistance(const WheelEncoderCounts& counts) {
    const int32_t left = counts.left - tracking.junctionApproachStart.left;
    const int32_t right = counts.right - tracking.junctionApproachStart.right;
    return getWheelOdometryGeometry().distanceMm((static_cast<float>(left) + right) * 0.5f);
}

static bool processJunction(uint8_t raw, unsigned long now) {
    switch (tracking.junctionPhase) {
        case JunctionPhase::Waiting: {
            if (tracking.junctionCoolingDown) {
                if (now - tracking.junctionFailedAt < JUNCTION_COOLDOWN_MS) return false;
                tracking.junctionCoolingDown = false;
            }
            const JunctionKind kind = detectJunction(raw);
            const bool nextIsTee = tracking.courseStep == COURSE_STEP_WAIT_TEE;
            const bool nextIsCross = tracking.courseStep == COURSE_STEP_WAIT_CROSS;
            const bool matching = (nextIsTee && (isLeftTurnJunction(kind) || kind == JUNCTION_CROSS)) ||
                                  (nextIsCross && (isRightTurnJunction(kind) || kind == JUNCTION_CROSS));

            if (!tracking.startupComplete || !matching) {
                tracking.junctionConfirmCount = 0;
                return false;
            }
            // Hai mau co the doi do rong, mien la cung chi vao buoc duong hien tai.
            if (tracking.junctionConfirmCount == 0) tracking.junctionApproachStart = getWheelEncoderCounts();
            if (++tracking.junctionConfirmCount < JUNCTION_CONFIRM_SAMPLES) return true;
            tracking.junctionConfirmCount = 0;
            if (!wheelEncodersReady()) {
                failJunction(now, "NO_ENCODER");
                return true;
            }

            tracking.junctionPhase = JunctionPhase::Approaching;
            tracking.junctionPhaseStartedAt = now;
            tracking.junctionTrailingSamples = 0;
            tracking.junctionMarkerMeasured = false;
            tracking.junctionTargetMm = LINE_SENSOR_FORWARD_MM + 0.5f * LINE_WIDTH_MM;
            tracking.confirmingCrossForward = false;
            tracking.awaitingCrossForward = false;
            tracking.appliedAction = LINE_STOP;
            tracking.searchingLine = false;
            tracking.lastLineDirection = nextIsTee ? -1 : 1;
            tracking.junctionScanDirection = tracking.lastLineDirection;
            tracking.steering.reset();
            speedPidReset(speedPidLeft);
            speedPidReset(speedPidRight);
            clearAnchor();
            const int degrees = nextIsTee ? JUNCTION_PIVOT_DEGREES : -JUNCTION_PIVOT_DEGREES;
            Serial.printf("[Junction] Nha %s COURSE=%d RAW=0x%02X -> tien den tam, quay %+d deg\n",
                          getJunctionName(kind), tracking.courseStep, raw, degrees);
            driveAutoWheels(MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
            return true;
        }
        case JunctionPhase::Approaching: {
            const WheelEncoderCounts counts = getWheelEncoderCounts();
            const int32_t left = counts.left - tracking.junctionApproachStart.left;
            const int32_t right = counts.right - tracking.junctionApproachStart.right;
            const auto& geometry = getWheelOdometryGeometry();
            const float traveled = junctionApproachDistance(counts);
            const bool wheelsMoving = left > 0 && right > 0;
            measureJunctionMarker(raw, traveled);
            if (wheelsMoving && traveled >= tracking.junctionTargetMm) {
                coastMotionAtJunction();
                tracking.junctionPhase = JunctionPhase::CenterSettling;
                tracking.junctionPhaseStartedAt = now;
                Serial.printf("[Junction] CENTER=%.1f mm TARGET=%.1f mm L=%ld R=%ld\n",
                              traveled, tracking.junctionTargetMm, static_cast<long>(left), static_cast<long>(right));
            } else if (now - tracking.junctionPhaseStartedAt >= JUNCTION_PIVOT_TIMEOUT_MS ||
                       (!wheelsMoving && geometry.distanceMm(std::max(left, right)) >= tracking.junctionTargetMm)) {
                Serial.printf("[Junction] APPROACH=%.1f mm RAW=0x%02X L=%ld R=%ld\n",
                              traveled, raw, static_cast<long>(left), static_cast<long>(right));
                failJunction(now, "APPROACH");
            }
            return true;
        }
        case JunctionPhase::CenterSettling:
            if (now - tracking.junctionPhaseStartedAt >= MOTOR_RAMP_MS) {
                tracking.junctionPhase = JunctionPhase::Pivoting;
                const float traveled = junctionApproachDistance(getWheelEncoderCounts());
                Serial.printf("[Junction] PIVOT_START=%.1f mm TARGET=%.1f mm\n",
                              traveled, tracking.junctionTargetMm);
                startEncoderPivot(-tracking.lastLineDirection * JUNCTION_PIVOT_DEGREES, now);
            }
            return true;
        case JunctionPhase::Pivoting: {
            const PivotResult result = updateEncoderPivot(now);
            if (result == PivotResult::Failed) {
                failJunction(now, "PIVOT");
            } else if (result == PivotResult::Reached) {
                tracking.junctionPhase = JunctionPhase::Reacquiring;
                tracking.junctionReacquireCount = 0;
                tracking.junctionPhaseStartedAt = now;
            }
            return true;
        }
        case JunctionPhase::Reacquiring: {
            const unsigned long elapsed = now - tracking.junctionPhaseStartedAt;
            if (elapsed < JUNCTION_POST_PIVOT_SETTLE_MS) return true;

            const int32_t difference = getWheelEncoderCounts().difference() -
                                       (tracking.pivot.startRight - tracking.pivot.startLeft);
            const float turned = getWheelOdometryGeometry().yawDegrees(difference);
            const float headingError = tracking.pivot.targetDegrees - turned;
            const bool headingNearTarget = std::abs(headingError) <= JUNCTION_LINE_YAW_DEG;
            float position;
            const bool narrowLine = detectJunction(raw) == JUNCTION_NONE && getLinePosition(raw, position);
            const bool onNewLine = headingNearTarget && narrowLine && (raw & 0x3C) != 0;
            if (onNewLine) {
                if (++tracking.junctionReacquireCount >= JUNCTION_REACQUIRE_SAMPLES) {
                    const bool leftTurn = tracking.courseStep == COURSE_STEP_WAIT_TEE;
                    tracking.courseStep = leftTurn ? COURSE_STEP_WAIT_CROSS : COURSE_STEP_STATIONS;
                    tracking.sequenceArmed = !leftTurn;
                    tracking.junctionPhase = JunctionPhase::Waiting;
                    tracking.searchingLine = false;
                    tracking.lostLineSamples = 0;
                    tracking.lineFoundSamples = 0;
                    Serial.printf("[Junction] LOCK RAW=0x%02X TURN=%+.1f deg MS=%lu\n", raw, turned, elapsed);
                    Serial.println(leftTurn ? "[Course] Da qua nga 3 (T), cho giao lo (+)"
                                            : "[Course] Da qua giao lo (+), cho tram servo");
                    return false;
                }
                coastMotionAtJunction();
            } else {
                tracking.junctionReacquireCount = 0;
            }

            if (elapsed >= JUNCTION_PIVOT_TIMEOUT_MS) {
                Serial.printf("[Junction] REACQUIRE RAW=0x%02X TURN=%+.1f deg\n", raw, turned);
                failJunction(now, "REACQUIRE");
            } else if (!headingNearTarget) {
                tracking.junctionScanDirection = headingError > 0.0f ? -1 : 1;
                const int turn = tracking.junctionScanDirection * JUNCTION_FINE_TURN_SPEED;
                driveAutoWheels(turn, -turn);
            } else if (!onNewLine && (raw == 0 || narrowLine)) {
                if (narrowLine) tracking.junctionScanDirection = position < 0.0f ? -1 : 1;
                const int turn = tracking.junctionScanDirection * JUNCTION_FINE_TURN_SPEED;
                driveAutoWheels(turn, -turn);
            } else if (!onNewLine) {
                driveAutoWheels(MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
            }
            return true;
        }
    }
    return false;
}
#else
static void startForwardRetry(LineAction action, bool confirmed) {
    clearAnchor();
    tracking.appliedAction = LINE_STOP;
    tracking.recovery = Recovery::Retry;
    tracking.retryElapsed = 0;
    tracking.retryAction = confirmed ? action : LINE_STOP;
    driveLine(LINE_FORWARD, motorBaseSpeed);
}

static bool recordBranchMotion(int left, int right, unsigned long now) {
    if (tracking.branchMotionCount > 0) {
        auto& last = tracking.branchHistory[tracking.branchMotionCount - 1];
        if (last.left == left && last.right == right) return true;
        last.duration = now - tracking.branchMotionStartedAt;
    }
    if (tracking.branchMotionCount == sizeof(tracking.branchHistory) / sizeof(tracking.branchHistory[0])) {
        return false;
    }
    auto& next = tracking.branchHistory[tracking.branchMotionCount++];
    next.left = left;
    next.right = right;
    next.duration = 0;
    tracking.branchMotionStartedAt = now;
    driveAutoWheels(left, right);
    return true;
}

static void reverseBranchMotion(unsigned long now) {
    const auto& motion = tracking.branchHistory[tracking.branchMotionCount - 1];
    tracking.branchMotionStartedAt = now;
    driveAutoWheels(-motion.left, -motion.right);
}

static void startBranchReturn(unsigned long now) {
    auto& last = tracking.branchHistory[tracking.branchMotionCount - 1];
    last.duration = now - tracking.branchMotionStartedAt;
    tracking.branchPhase = BranchPhase::Returning;
    reverseBranchMotion(now);
    Serial.printf("[Branch] Lui ve nga re, %u doan motor\n", tracking.branchMotionCount);
}

// Nhan nhanh trai khi S1..S4 den, S6..S8 trang; S5 co the con nam tren line giua.
static bool processFirstBranch(uint8_t raw, LineAction action, bool confirmed, unsigned long now) {
    // NHANH 3 GIA: S1..S4 den, S5 VAN DEN (duong chinh con nam duoi xe),
    // S6..S8 trang. Do S5 con den nen day KHONG phai nga re 90 do - nga re
    // 90 do (S5 trang) duoc processJunction() xu ly truoc.
    const bool candidate = (raw & 0x0F) == 0x0F && (raw & 0xE0) == 0 && (raw & 0x10) != 0;
    switch (tracking.branchPhase) {
        case BranchPhase::Waiting:
            if (!tracking.startupComplete || !candidate || !tracking.previousBranchCandidate) {
                tracking.previousBranchCandidate = candidate;
                return false;
            }
            clearAnchor();
            tracking.awaitingCrossForward = false;
            tracking.confirmingCrossForward = false;
            tracking.sequenceArmed = false;
            tracking.confirmingCrossClear = false;
            tracking.branchPhase = BranchPhase::Turning;
            tracking.branchTurnStartedAt = now;
            tracking.branchLeftMainLine = (raw & 0x18) != 0x18;
            recordBranchMotion(0, motorBaseSpeed, now);
            Serial.printf("[Branch] Re trai vao nhanh dau tien\n");
            return true;
        case BranchPhase::Turning: {
            if ((raw & 0x18) != 0x18) tracking.branchLeftMainLine = true;
            const bool centered = action == LINE_FORWARD && !candidate && (raw & 0x18) == 0x18;
            if (centered && tracking.previousBranchCenter) {
                tracking.branchPhase = BranchPhase::Following;
                recordBranchMotion(motorBaseSpeed, motorBaseSpeed, now);
            } else if (now - tracking.branchTurnStartedAt >= FIRST_BRANCH_TURN_TIMEOUT_MS) {
                startBranchReturn(now);
            }
            tracking.previousBranchCenter = centered;
            return true;
        }
        case BranchPhase::Following:
            if (!confirmed) return true;
            if (action == LINE_STOP) {
                startBranchReturn(now);
            } else {
                const int left = action == LINE_LEFT ? 0 : motorBaseSpeed;
                const int right = action == LINE_RIGHT ? 0 : motorBaseSpeed;
                // Day bo nho thi quay ve theo cac doan da ghi, tranh mat dau duong ve.
                if (!recordBranchMotion(left, right, now)) startBranchReturn(now);
            }
            return true;
        case BranchPhase::Returning: {
            const auto& motion = tracking.branchHistory[tracking.branchMotionCount - 1];
            if (now - tracking.branchMotionStartedAt < motion.duration) return true;
            --tracking.branchMotionCount;
            if (tracking.branchMotionCount > 0) {
                reverseBranchMotion(now);
            } else {
                tracking.branchPhase = BranchPhase::Complete;
                startForwardRetry(action, confirmed);
                Serial.printf("[Branch] Da ve nga re, thu di thang\n");
            }
            return true;
        }
        case BranchPhase::Complete:
            return false;
    }
    return false;
}

static void startUndoEntry(unsigned long now) {
    tracking.recovery = Recovery::UndoEntry;
    tracking.phaseStartedAt = now;
    driveAutoWheels(-tracking.entryWheels.left, -tracking.entryWheels.right);
}

#endif

#ifdef SENSOR_PHOTORESISTOR
// Vach chi cham o mep (S1/S2 hoac S7/S8): dung toc do "mep" de sua lech nhe,
// nhung luon giu banh trong tren MOTOR_MIN_SPEED de banh khong bao gio dung that.
// Spec yeu cau "ca hai banh van tien" khi lech nhe sang mot ben.
static bool photoresEdgeWheels(uint8_t raw, int speed, WheelSpeeds& wheels) {
    const bool onLeftEdge = raw == 0x01 || raw == 0x02 || raw == 0x03;
    const bool onRightEdge = raw == 0x40 || raw == 0x80 || raw == 0xC0;
    if (!onLeftEdge && !onRightEdge) return false;

    const int maxInner = MOTOR_MAX_SPEED - MOTOR_EDGE_SPEED_DELTA;
    const int inner = std::max(MOTOR_MIN_SPEED, std::min(maxInner, speed - MOTOR_EDGE_SPEED_DELTA / 2));
    const int outer = inner + MOTOR_EDGE_SPEED_DELTA;
    wheels = onLeftEdge ? WheelSpeeds{inner, outer} : WheelSpeeds{outer, inner};
    return true;
}

// Mat vach: xoay tai cho sang phia vach vua mat de tim lai duong.
// Buoc 1: giu nguyen lenh cua vong do line trong 1 mau (tranh giat).
// Buoc 2: xoay tai cho sang phia vach vua mat (khong bao gio dung hanh).
static void searchForLine() {
    if (tracking.searchingLine) return;
    tracking.searchingLine = true;
    tracking.recovery = Recovery::Follow;
    tracking.awaitingCrossForward = false;
    tracking.appliedAction = LINE_STOP;
    tracking.steering.reset();
    clearAnchor();

    const int turnSpeed = tracking.lastLineDirection * MOTOR_MIN_SPEED;
    driveAutoWheels(turnSpeed, -turnSpeed);
}
#endif

#ifndef SENSOR_PHOTORESISTOR
static void startRecovery(unsigned long now) {
    tracking.steering.reset();
    tracking.entryDuration = (tracking.followingStraight ? tracking.straightStartedAt : now)
                             - tracking.anchorStartedAt;
    if (!tracking.followingStraight) {
        startUndoEntry(now);
        return;
    }
    tracking.straightDuration = now - tracking.straightStartedAt;
    tracking.recovery = Recovery::UndoStraight;
    tracking.phaseStartedAt = now;
    driveAutoWheels(-tracking.straightWheels.left, -tracking.straightWheels.right);
}

#endif

static void followLine(uint8_t raw, LineAction action, unsigned long now) {
#ifdef SENSOR_PHOTORESISTOR
    // Tim thay vach -> dung che do xoay tim lai, va nho phia vach vua mat.
    tracking.searchingLine = false;
    tracking.lostLineSamples = 0;
    tracking.lineFoundSamples = 0;
    float position;
    if (getLinePosition(raw, position) && position != 0.0f) {
        tracking.lastLineDirection = position < 0.0f ? -1 : 1;
    }
#endif
    // Vach xuat phat chi dua xe di thang, chua phai moc lui hay tram servo.
    if (!tracking.startupComplete && action == LINE_CROSS) {
        clearAnchor();
        tracking.steering.reset();
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
    WheelSpeeds wheels;
#ifdef SENSOR_PHOTORESISTOR
    // Vach o mep: giu banh trong tren MOTOR_MIN_SPEED de banh khong bao gio
    // dung that (spec: "ca hai banh van tien").
    if (!photoresEdgeWheels(raw, speed, wheels)) {
        wheels = tracking.steering.update(raw, speed, now);
    } else {
        tracking.steering.reset();
    }
#else
    wheels = tracking.steering.update(raw, speed, now);
#endif
    if (tracking.anchor != LINE_STOP) {
        if (tracking.followingStraight) {
            tracking.straightWheels = wheels;
        } else {
            tracking.entryWheels = wheels;
        }
    }
#ifdef SENSOR_PHOTORESISTOR
    driveFollowing(wheels.left, wheels.right, now);
#else
    driveAutoWheels(wheels.left, wheels.right);
#endif
}

#ifndef SENSOR_PHOTORESISTOR
static void updateRecovery(uint8_t raw, LineAction action, bool confirmed, unsigned long now, unsigned long elapsed) {
    switch (tracking.recovery) {
        case Recovery::UndoStraight:
            if (now - tracking.phaseStartedAt >= tracking.straightDuration) startUndoEntry(now);
            return;
        case Recovery::UndoEntry:
            if (now - tracking.phaseStartedAt < tracking.entryDuration) return;
            startForwardRetry(action, confirmed);
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
#endif

static void trackEightSensors(uint8_t raw, LineAction action, bool confirmed, unsigned long now,
                              unsigned long elapsed) {
    const bool crossSample = action == LINE_CROSS || isCrossPattern(raw);

#ifdef SENSOR_PHOTORESISTOR
    (void)elapsed;
    if (!tracking.startupComplete) {
        float position;
        const bool centered = (raw & 0x18) != 0 && detectJunction(raw) == JUNCTION_NONE &&
                              getLinePosition(raw, position);
        if (centered) {
            tracking.startupCenterSeenAt = now;
            if (!tracking.confirmingCrossClear) {
                tracking.confirmingCrossClear = true;
                tracking.crossClearStartedAt = now;
            } else if (now - tracking.crossClearStartedAt >= CROSS_FORWARD_CONFIRM_MS) {
                tracking.confirmingCrossClear = false;
                tracking.startupComplete = true;
                Serial.println("[Course] Da roi vach xuat phat, cho nga 3 (T)");
            }
        } else if (raw != 0 || now - tracking.startupCenterSeenAt > LINE_SAMPLE_INTERVAL_MS) {
            tracking.confirmingCrossClear = false;
        }
    } else if (tracking.courseStep == COURSE_STEP_STATIONS && !tracking.sequenceArmed) {
        if (action != LINE_FORWARD || detectJunction(raw) != JUNCTION_NONE) {
            tracking.confirmingCrossClear = false;
        } else if (confirmed) {
            if (!tracking.confirmingCrossClear) {
                tracking.confirmingCrossClear = true;
                tracking.crossClearStartedAt = now;
            } else if (now - tracking.crossClearStartedAt >= CROSS_FORWARD_CONFIRM_MS) {
                tracking.confirmingCrossClear = false;
                tracking.sequenceArmed = true;
            }
        }
    }

    if (processJunction(raw, now)) return;

    if (action != LINE_FORWARD) tracking.confirmingCrossForward = false;
    if (action == LINE_STOP) {
        tracking.awaitingCrossForward = false;
        tracking.lineFoundSamples = 0;
        // Mot mau mat vach giu nguyen PWM hien tai va tich phan PI.
        if (!tracking.searchingLine && ++tracking.lostLineSamples >= 2) searchForLine();
        return;
    }
    tracking.lostLineSamples = 0;
    if (tracking.searchingLine && ++tracking.lineFoundSamples < 2) return;
    if (!confirmed) return;

    const bool stationArmed = tracking.courseStep == COURSE_STEP_STATIONS && tracking.sequenceArmed;
#else
    if (tracking.recovery != Recovery::Follow) {
        tracking.previousBranchCandidate = false;
        tracking.confirmingCrossClear = false;
        updateRecovery(raw, action, confirmed, now, elapsed);
        return;
    }
    if (processFirstBranch(raw, action, confirmed, now)) {
        tracking.steering.reset();
        return;
    }

    if (action != LINE_FORWARD) {
        tracking.confirmingCrossForward = false;
        tracking.confirmingCrossClear = false;
    }
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
        tracking.steering.reset();
        stopMotion();
        return;
    }
    if (!confirmed) return;
    if (action == LINE_STOP) {
        tracking.awaitingCrossForward = false;
        startRecovery(now);
        return;
    }

    const bool stationArmed = tracking.sequenceArmed;
#endif
    if (stationArmed && tracking.completedSequences < CROSS_SEQUENCE_LIMIT &&
        crossSample && tracking.appliedAction == LINE_FORWARD) {
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
            tracking.steering.reset();
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
#endif

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
#ifdef SENSOR_PHOTORESISTOR
        if (!tracking.enabled && wheelEncodersReady()) {
            resetWheelEncoderCounts();
            Serial.println("[Encoder] START YAW=0.0 deg");
        }
        speedPidReset(speedPidLeft);
        speedPidReset(speedPidRight);
#endif
        tracking = {};
        tracking.enabled = true;
        tracking.activeLevel = lineActiveState;
        tracking.sampledAt = now;
#if NUM_SENSORS == 8
        homeCrossServos(now);
#endif
        return;
    }

#ifdef SENSOR_PHOTORESISTOR
    // PHUC HOI sau khi quay tai cho that bai (mat encoder / het timeout).
    // Truoc day dong "return;" o day lam robot DONG VINH VIEN: khong con
    // phan hoi mau cam bien nua, chi khoi dong lai bang cach bam '1' hai lan.
    // Bay gio: dung motor an toan, bo qua chuoi tram, va TIEP TUC do line.
    if (tracking.sequenceTurnFailed) {
        tracking.sequenceTurnFailed = false;
        tracking.sequenceStep = 0;
        tracking.appliedAction = LINE_STOP;
        tracking.steering.reset();
        speedPidReset(speedPidLeft);
        speedPidReset(speedPidRight);
        stopMotion();
        Serial.println("[Turn] Turn that bai, quay lai do line");
    }
#endif

#if NUM_SENSORS == 8
    if (tracking.settlingServos) {
        if (now - tracking.servosStartedAt < CROSS_SERVO_SETTLE_MS) return;
        tracking.settlingServos = false;
    }
    if (tracking.sequenceStep != 0) {
        updateCrossSequence(now);
        return;
    }
#endif

    const unsigned long elapsed = now - tracking.sampledAt;
    if (elapsed < LINE_SAMPLE_INTERVAL_MS) return;
    tracking.sampledAt = now;
    const uint8_t raw = readRawSensors();
#ifdef SENSOR_PHOTORESISTOR
    const LineAction action = isCrossPattern(raw) ? LINE_CROSS : getLineAction(raw);
#else
    const LineAction action = getLineAction(raw);
#endif
    const bool confirmed = action == tracking.previousSample;
    tracking.previousSample = action;
#if NUM_SENSORS == 5
    trackFiveSensors(action, confirmed, now);
#else
    trackEightSensors(raw, action, confirmed, now, elapsed);
#endif
}
