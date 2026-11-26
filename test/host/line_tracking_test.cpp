#include "line_tracking.h"
#include "line_sensor.h"
#include "motor_driver.h"
#include "servo2.h"
#include "servo3.h"

#include <cassert>
#ifdef NDEBUG
#error Host regression tests require assertions
#endif
#include <cstdio>
#include <vector>

HostSerial Serial;
int motorBaseSpeed = MOTOR_BASE_SPEED;

static unsigned long now = 1000;
static uint8_t sensorMask = 0;
static int left = 0;
static int right = 0;
static int servo2Angle = 180;
static int servo3Angle = 180;

struct MotorEvent {
    unsigned long at;
    int left;
    int right;
};
struct ServoEvent {
    unsigned long at;
    int servo;
    int angle;
};
static std::vector<MotorEvent> motorEvents;
static std::vector<ServoEvent> servoEvents;

unsigned long millis() { return now; }

int digitalRead(uint8_t pin) {
    const uint8_t pins[] = { PIN_LINE_S1, PIN_LINE_S2, PIN_LINE_S3, PIN_LINE_S4, PIN_LINE_S5
#if NUM_SENSORS == 8
                             , PIN_LINE_S6, PIN_LINE_S7, PIN_LINE_S8
#endif
    };
    for (unsigned int i = 0; i < NUM_SENSORS; ++i) {
        if (pins[i] == pin) return (sensorMask & (1U << i)) ? lineActiveState : !lineActiveState;
    }
    assert(false);
    return LOW;
}

void runLR(int newLeft, int newRight, int power) {
    assert(power == MOTOR_OUTPUT_PERCENT);
    if (left != newLeft || right != newRight) motorEvents.push_back({now, newLeft, newRight});
    left = newLeft;
    right = newRight;
}

void stopMotion() { runLR(0, 0, MOTOR_OUTPUT_PERCENT); }
int servo2_getAngle() { return servo2Angle; }
int servo3_getAngle() { return servo3Angle; }

void servo2_setAngle(int angle) {
    servo2Angle = angle;
    servoEvents.push_back({now, 2, angle});
}

void servo3_setAngle(int angle) {
    servo3Angle = angle;
    servoEvents.push_back({now, 3, angle});
}

static void hold(uint8_t mask, unsigned int samples = 1) {
    sensorMask = mask;
    for (unsigned int i = 0; i < samples; ++i) {
        now += LINE_SAMPLE_INTERVAL_MS;
        updateLineTracking(true);
    }
}

static void checkMotor(int expectedLeft, int expectedRight, int line) {
    if (left != expectedLeft || right != expectedRight) {
        std::fprintf(stderr, "Line %d at %lu ms: expected (%d, %d), got (%d, %d)\n",
                     line, now, expectedLeft, expectedRight, left, right);
    }
    assert(left == expectedLeft && right == expectedRight);
}
#define expectMotor(left, right) checkMotor(left, right, __LINE__)

static void startTracking() {
    updateLineTracking(false);
    lineActiveState = LINE_ACTIVE_STATE;
    servo2Angle = servo3Angle = 180;
    motorEvents.clear();
    servoEvents.clear();
    updateLineTracking(true);
}

#if NUM_SENSORS == 8
constexpr uint8_t straight = 0x18;
constexpr uint8_t cross = 0xFF;
constexpr uint8_t leftOnly = 0x01;
constexpr uint8_t rightOnly = 0x80;

static void passBends() {
    hold(leftOnly, 8);
    hold(straight, 20);
    hold(rightOnly, 8);
    hold(straight, 22);
}

static void testAllMasks() {
    for (unsigned int raw = 0; raw <= cross; ++raw) {
        LineAction expected = LINE_STOP;
        if ((raw & 0x1F) == 0x1F) expected = LINE_CROSS;
        else if (raw == 0xF8 || raw == 0xFC) expected = LINE_RIGHT;
        else if ((raw & 0x0F) && (raw & 0xF0)) expected = LINE_FORWARD;
        else if (raw & 0x0F) expected = LINE_LEFT;
        else if (raw & 0xF0) expected = LINE_RIGHT;
        assert(getLineAction(raw) == expected);
    }
}

static void testTurnRecovery() {
    startTracking();
    hold(leftOnly, 3);
    assert(left == 0 && right > 0);
    hold(0);
    assert(left == 0 && right > 0);
    hold(leftOnly, 2);
    assert(left == 0 && right > 0);
    hold(0, 2);
    assert(left == 0 && right < 0);
    const auto turnDuration = motorEvents[1].at - motorEvents[0].at;
    hold(leftOnly, turnDuration / LINE_SAMPLE_INTERVAL_MS);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(leftOnly, LINE_RECOVERY_FORWARD_MS / LINE_SAMPLE_INTERVAL_MS);
    expectMotor(0, 0);
    hold(leftOnly, 30);
    expectMotor(0, 0);
    hold(straight, 2);
    expectMotor(motorBaseSpeed, motorBaseSpeed);

    startTracking();
    hold(rightOnly, 2);
    assert(left > 0 && right == 0);
    hold(0, 2);
    assert(left < 0 && right == 0);
    updateLineTracking(false);
    expectMotor(0, 0);
    hold(0, 25);
    expectMotor(0, 0);
}

static void testRecoveryAfterStraight() {
    startTracking();
    hold(leftOnly, 4);
    const auto turnStart = now - 2 * LINE_SAMPLE_INTERVAL_MS;
    hold(straight, 5);
    const auto turnEnd = now - 3 * LINE_SAMPLE_INTERVAL_MS;
    const auto straightLeft = left, straightRight = right;
    hold(0, 2);
    expectMotor(-straightLeft, -straightRight);
    const auto straightDuration = now - turnEnd;
    const auto turnDuration = turnEnd - turnStart;
    hold(0, straightDuration / LINE_SAMPLE_INTERVAL_MS);
    assert(left == 0 && right < 0);
    hold(leftOnly, turnDuration / LINE_SAMPLE_INTERVAL_MS);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(cross, 50);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(straight, 20);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    assert(servoEvents.empty());

    startTracking();
    hold(straight, 22);
    hold(cross, 30);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(0, 2);
    expectMotor(-motorBaseSpeed, -motorBaseSpeed);
    hold(0, 31);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(straight, 20);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    assert(servoEvents.empty());

    startTracking();
    hold(rightOnly, 3);
    hold(straight, 20);
    hold(0);
    expectMotor(0, 0);
}

static void testCrossSequence() {
    motorBaseSpeed = MOTOR_PIVOT_SPEED + MOTOR_SPEED_STEP;
    startTracking();
    updateLineTracking(false);
    servo2Angle = servo3Angle = 90;
    updateLineTracking(true);
    assert(servo2Angle == 180 && servo3Angle == 180);
    hold(straight, CROSS_SERVO_SETTLE_MS / LINE_SAMPLE_INTERVAL_MS);
    expectMotor(0, 0);
    hold(straight);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    servoEvents.clear();
    hold(straight, 20);
    passBends();

    assert(getCrossReverseDuration() == CROSS_REVERSE_DEFAULT_MS);
    constexpr uint32_t configuredDuration = 750;
    setCrossReverseDuration(configuredDuration);
    hold(cross, 30);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(straight, 10);
    hold(cross);
    hold(straight, 21);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    motorEvents.clear();
    hold(straight);
    expectMotor(MOTOR_PIVOT_SPEED, -MOTOR_PIVOT_SPEED);
    const auto startedAt = now;
    setCrossReverseDuration(2500);
    hold(0, 1000);
    expectMotor(0, 0);

    const int speed = motorBaseSpeed;
    const MotorEvent expected[] = {
        {0, MOTOR_PIVOT_SPEED, -MOTOR_PIVOT_SPEED},
        {2 * CROSS_RIGHT_45_MS, 0, 0},
        {MOTOR_RAMP_MS, -speed, -speed},
        {configuredDuration, 0, 0},
        {MOTOR_RAMP_MS + 2 * CROSS_SERVO_SETTLE_MS, MOTOR_PIVOT_SPEED, -MOTOR_PIVOT_SPEED},
        {CROSS_RIGHT_45_MS, 0, 0},
        {MOTOR_RAMP_MS + 2 * CROSS_SERVO_SETTLE_MS, -MOTOR_PIVOT_SPEED, MOTOR_PIVOT_SPEED},
        {CROSS_LEFT_45_MS, 0, 0},
        {MOTOR_RAMP_MS, speed, speed},
        {configuredDuration, 0, 0},
        {MOTOR_RAMP_MS, -MOTOR_PIVOT_SPEED, MOTOR_PIVOT_SPEED},
        {2 * CROSS_LEFT_45_MS, 0, 0},
    };
    assert(motorEvents.size() == sizeof(expected) / sizeof(expected[0]));
    auto prevAt = startedAt;
    for (unsigned int i = 0; i < motorEvents.size(); ++i) {
        assert(motorEvents[i].at - prevAt == expected[i].at);
        assert(motorEvents[i].left == expected[i].left && motorEvents[i].right == expected[i].right);
        prevAt = motorEvents[i].at;
    }
    assert(servoEvents.size() == 4);
    assert(servoEvents[0].servo == 2 && servoEvents[0].angle == 0);
    assert(servoEvents[1].servo == 3 && servoEvents[1].angle == 45);
    assert(servoEvents[2].servo == 3 && servoEvents[2].angle == 180);
    assert(servoEvents[3].servo == 2 && servoEvents[3].angle == 180);
    assert(servoEvents[1].at - servoEvents[0].at == CROSS_SERVO_SETTLE_MS);
    assert(servoEvents[3].at - servoEvents[2].at == CROSS_SERVO_SETTLE_MS);
    hold(straight, 22);
    expectMotor(speed, speed);

    hold(cross, 2);
    hold(straight, 22);
    expectMotor(MOTOR_PIVOT_SPEED, -MOTOR_PIVOT_SPEED);
    lineActiveState = !lineActiveState;
    hold(0);
    expectMotor(0, 0);
    hold(0, 30);
    expectMotor(0, 0);
}

static void testStartBar() {
    motorBaseSpeed = MOTOR_BASE_SPEED;
    startTracking();
    hold(cross, 30);
    hold(0, 2);
    expectMotor(0, 0);
    hold(straight, 5);
    hold(cross, 2);
    hold(straight, 22);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    assert(servoEvents.empty());
    for (const auto& event : motorEvents) {
        assert(event.left >= 0 && event.right >= 0);
    }

    // Sau khi roi start, giao lo mat line van lui tim line.
    hold(cross, 3);
    hold(0, 2);
    expectMotor(-motorBaseSpeed, -motorBaseSpeed);
}

static void testThreeServoStations() {
    startTracking();
    setCrossReverseDuration(100);
    hold(cross, 5);
    hold(straight, 22);

    // Goc re chi thay mot ben se khong tinh thanh tram servo.
    hold(leftOnly, 3);
    hold(straight, 25);
    hold(rightOnly, 3);
    hold(straight, 25);
    assert(servoEvents.empty());

    hold(cross, 5);
    hold(straight, 22);
    assert(servoEvents.empty());
    passBends();
    for (int corner = 0; corner < 2; ++corner) {
        hold(0xF8, 3);
        hold(0, 5);
        expectMotor(MOTOR_PIVOT_SPEED, -MOTOR_PIVOT_SPEED);
        hold(straight, 22);
        assert(servoEvents.empty());
    }
    for (unsigned int station = 0; station < 3; ++station) {
        hold(station == 0 ? 0x1F : cross, 5);
        hold(straight, 22);
        expectMotor(MOTOR_PIVOT_SPEED, -MOTOR_PIVOT_SPEED);
        hold(cross, 600);
        assert(servoEvents.size() == (station + 1) * 4);

        // Doc lai cung vach sau khi quay ve khong lap chuoi.
        hold(straight, 5);
        hold(cross, 5);
        hold(straight, 22);
        expectMotor(motorBaseSpeed, motorBaseSpeed);
        assert(servoEvents.size() == (station + 1) * 4);
    }

    hold(cross, 5);
    hold(straight, 100);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    assert(servoEvents.size() == 12);

    // The end bar followed by a gap latches stop until a new run.
    hold(cross, 5);
    hold(0, 2);
    expectMotor(0, 0);

    hold(straight, 25);
    expectMotor(0, 0);

    // Bat lai che do tu dong tao mot luot chay moi.
    startTracking();
    hold(straight, 22);
    passBends();
    hold(cross, 5);
    hold(straight, 22);
    expectMotor(MOTOR_PIVOT_SPEED, -MOTOR_PIVOT_SPEED);
}

#endif

int main() {
#if NUM_SENSORS == 8 && !defined(SENSOR_PHOTORESISTOR)
#ifdef BOARD_ESP32_38PIN
    static_assert(PIN_LINE_S1 == 16 && PIN_LINE_S8 == 23 && PIN_LINE_IR_EN == 4, "ESP32 IR orientation");
#else
    static_assert(PIN_LINE_S1 == 18 && PIN_LINE_S8 == 4 && PIN_LINE_IR_EN == 1, "S3 IR orientation");
#endif
#endif
#if NUM_SENSORS == 8
    testAllMasks();
    startTracking();
    hold(0x0C, 4);
    assert(left > 0 && left < right);
    startTracking();
    hold(0x30, 4);
    assert(right > 0 && right < left);
    startTracking();
    hold(0xF8, 3);
    hold(0, 4);
    expectMotor(MOTOR_PIVOT_SPEED, -MOTOR_PIVOT_SPEED);
    hold(straight, 2);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    testTurnRecovery();
    testRecoveryAfterStraight();
    testCrossSequence();
    testStartBar();
    testThreeServoStations();
#else
    startTracking();
    hold(0x04, 2);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(SENSOR_MASK, 16);
    expectMotor(0, 0);
    hold(0);
    expectMotor(0, 0);
    assert(servoEvents.empty());
#endif
}
