#include "line_tracking.h"
#include "line_sensor.h"
#include "motor_driver.h"
#include "servo2.h"
#include "servo3.h"
#ifdef SENSOR_PHOTORESISTOR
#include "wheel_encoders.h"
#endif
#include "junction_detect.h"
#include "speed_pid.h"

#include <cassert>
#include <cmath>
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

static const uint8_t sensorPins[NUM_SENSORS] = {
    PIN_LINE_S1, PIN_LINE_S2, PIN_LINE_S3, PIN_LINE_S4, PIN_LINE_S5
#if NUM_SENSORS == 8
    , PIN_LINE_S6, PIN_LINE_S7, PIN_LINE_S8
#endif
};

static unsigned int sensorIndex(uint8_t pin) {
    for (unsigned int i = 0; i < NUM_SENSORS; ++i) {
        if (sensorPins[i] == pin) return i;
    }
    assert(false);
    return 0;
}

#ifdef SENSOR_PHOTORESISTOR
static const uint16_t* adcSamples = nullptr;
static unsigned int adcReadCount = 0;
static bool encoderTestEnabled = false;
static WheelEncoderCounts encoderTestCounts{};
static WheelEncoderVelocity encoderTestVelocity{};

bool wheelEncodersReady() { return encoderTestEnabled; }
void resetWheelEncoderCounts() { encoderTestCounts = {}; }
WheelEncoderCounts getWheelEncoderCounts() { return encoderTestCounts; }
WheelEncoderVelocity getWheelEncoderVelocity(unsigned long) { return encoderTestVelocity; }

const WheelOdometryGeometry& getWheelOdometryGeometry() {
    static constexpr WheelOdometryGeometry geometry{
        FRONT_WHEEL_DIAMETER_MM, FRONT_TRACK_WIDTH_MM, ENCODER_PULSES_PER_REV
    };
    return geometry;
}

uint16_t analogRead(uint8_t pin) {
    ++adcReadCount;
    const unsigned int i = sensorIndex(pin);
    if (adcSamples) return adcSamples[i];
    const bool black = (sensorMask & (1U << i)) != 0;
    const bool high = (lineActiveState == HIGH) == black;
    return high ? PHOTORES_ADC_THRESHOLDS[i] : PHOTORES_ADC_THRESHOLDS[i] - 1;
}

static void testPhotoresThresholds() {
    uint16_t samples[NUM_SENSORS];
    for (int i = 0; i < NUM_SENSORS; ++i) {
        samples[i] = PHOTORES_ADC_THRESHOLDS[i] - 1;
    }
    samples[3] = PHOTORES_ADC_THRESHOLDS[3];
    samples[4] = PHOTORES_ADC_THRESHOLDS[4] + 1;
    adcSamples = samples;

    uint16_t logged[NUM_SENSORS];
    lineActiveState = LINE_ACTIVE_STATE;
    const uint8_t blackMask = readPhotoresSensors(logged);
    assert(blackMask == 0xE7);
    assert(readRawSensors() == blackMask);
    assert(getLineAction(blackMask) == LINE_FORWARD);
    for (int i = 0; i < NUM_SENSORS; ++i) assert(logged[i] == samples[i]);

    toggleLineActiveState();
    assert(readRawSensors() == 0x18);
    lineActiveState = LINE_ACTIVE_STATE;
    adcSamples = nullptr;
}
#else
int digitalRead(uint8_t pin) {
    const bool black = (sensorMask & (1U << sensorIndex(pin))) != 0;
    return black ? lineActiveState : !lineActiveState;
}
#endif

void runLR(int newLeft, int newRight, int power) {
    int expectedPower = newLeft == newRight ? MOTOR_OUTPUT_PERCENT : MOTOR_TURN_OUTPUT_PERCENT;
    if (power == 100 && newRight == -newLeft && newLeft != 0) {
        // Quay theo encoder dung duty truc tiep.
        expectedPower = 100;
    }
    if (power != expectedPower) {
        std::fprintf(stderr, "Power mismatch: got %d expected %d for (%d, %d)\n",
                     power, expectedPower, newLeft, newRight);
    }
    assert(power == expectedPower);
    if (left != newLeft || right != newRight) motorEvents.push_back({now, newLeft, newRight});
    left = newLeft;
    right = newRight;
}

void stopMotion() { runLR(0, 0, MOTOR_OUTPUT_PERCENT); }
#ifdef SENSOR_PHOTORESISTOR
void coastMotionAtJunction() { stopMotion(); }
#endif
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

static void holdFor(uint8_t mask, unsigned long durationMs) {
    hold(mask, (durationMs + LINE_SAMPLE_INTERVAL_MS - 1) / LINE_SAMPLE_INTERVAL_MS);
}

static void holdConfirmedStraight() {
    hold(straight, CROSS_FORWARD_CONFIRM_MS / LINE_SAMPLE_INTERVAL_MS + 2);
}

#ifdef SENSOR_PHOTORESISTOR
static void testPhotoresSampleInterval() {
    startTracking();
    sensorMask = straight;
    const unsigned int before = adcReadCount;
    now += 4;
    updateLineTracking(true);
    assert(adcReadCount == before);
    ++now;
    updateLineTracking(true);
    assert(adcReadCount == before + NUM_SENSORS);
    now += 5;
    updateLineTracking(true);
    assert(adcReadCount == before + 2 * NUM_SENSORS);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
}

static void testPhotoresEdgesAndSearch() {
    const int inner = MOTOR_MIN_SPEED;
    const int outer = inner + MOTOR_EDGE_SPEED_DELTA;
    for (const uint8_t edge : {0x01, 0x02, 0x03, 0x40, 0x80, 0xC0}) {
        startTracking();
        const bool onLeft = edge <= 0x03;
        const int expectedLeft = onLeft ? inner : outer;
        const int expectedRight = onLeft ? outer : inner;
        const int turnSpeed = onLeft ? -MOTOR_MIN_SPEED : MOTOR_MIN_SPEED;

        hold(edge, 2);
        expectMotor(expectedLeft, expectedRight);
        hold(0);
        expectMotor(expectedLeft, expectedRight);
        hold(0);
        expectMotor(turnSpeed, -turnSpeed);
        hold(edge);
        expectMotor(turnSpeed, -turnSpeed);
        hold(edge);
        expectMotor(expectedLeft, expectedRight);
        hold(straight, 2);
        expectMotor(motorBaseSpeed, motorBaseSpeed);
    }

    startTracking();
    hold(0);
    expectMotor(0, 0);
    hold(0);
    expectMotor(-MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
    updateLineTracking(false);
    expectMotor(0, 0);
}

static void advanceToJunctionCenter(int pulsesAlreadyTraveled = 0) {
    expectMotor(MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
    encoderTestCounts.left += 2 - pulsesAlreadyTraveled;
    encoderTestCounts.right += 2 - pulsesAlreadyTraveled;
    hold(0, 2); // Do duoc canh sau cach canh dau khoang 20.7 mm.
    encoderTestCounts.left += 10;
    encoderTestCounts.right += 11;
    hold(0); // 12.5 xung trung binh, van thieu moc 130.4 mm.
    expectMotor(MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
    ++encoderTestCounts.left;
    hold(0);
    expectMotor(0, 0);
    holdFor(0, MOTOR_RAMP_MS - LINE_SAMPLE_INTERVAL_MS);
    expectMotor(0, 0);
    hold(0);
}

static void completeRouteTurn(uint8_t first, uint8_t second, int direction) {
    hold(first);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    ++encoderTestCounts.left;
    ++encoderTestCounts.right;
    hold(second);
    advanceToJunctionCenter(1);
    expectMotor(-direction * MOTOR_MAX_SPEED, direction * MOTOR_MAX_SPEED);
    encoderTestCounts.left -= direction * 8;
    encoderTestCounts.right += direction * 9;
    hold(second);
    expectMotor(-direction * JUNCTION_FINE_TURN_SPEED, direction * JUNCTION_FINE_TURN_SPEED);
    encoderTestCounts.left -= direction * 2;
    encoderTestCounts.right += direction;
    hold(second);
    expectMotor(0, 0);

    // Mau giao lo con duoi thanh cam bien khong duoc tinh la vach moi.
    holdFor(second, JUNCTION_POST_PIVOT_SETTLE_MS - LINE_SAMPLE_INTERVAL_MS);
    expectMotor(0, 0);
    hold(second);
    expectMotor(MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
    hold(straight);
    expectMotor(0, 0);
    hold(straight);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
}

static void preparePhotoresStations() {
    const bool encodersWereEnabled = encoderTestEnabled;
    encoderTestEnabled = true;
    holdConfirmedStraight();
    completeRouteTurn(0x07, 0x1F, 1);
    completeRouteTurn(0x3C, 0xFF, -1);
    encoderTestEnabled = encodersWereEnabled;
}

static void approachEncoderTurn() {
    startTracking();
    assert(encoderTestCounts.left == 0 && encoderTestCounts.right == 0);
    preparePhotoresStations();
    hold(cross, 5);
    holdConfirmedStraight();
    expectMotor(MOTOR_ENCODER_TURN_SPEED, -MOTOR_ENCODER_TURN_SPEED);
}

static void testEncoderHeadingAndTurn() {
    const auto& geometry = getWheelOdometryGeometry();
    assert(geometry.distanceMm(11.5f) < LINE_SENSOR_FORWARD_MM);
    assert(geometry.distanceMm(12) >= LINE_SENSOR_FORWARD_MM);
    assert(std::fabs(geometry.yawDegrees(1) - 4.4328f) < 0.001f);
    assert(geometry.turnPulses(45) == 10 && geometry.turnPulses(90) == 20);
    assert(std::fabs(lineBearingDegrees(21.0f, LINE_SENSOR_FORWARD_MM) - 9.9262f) < 0.001f);

    encoderTestEnabled = true;
    encoderTestCounts = {18, 11};
    approachEncoderTurn();
    const auto turnStart = encoderTestCounts;

    // Van dang quay sau thoi gian cu neu encoder chua du xung.
    hold(0, 2 * CROSS_TURN_45_MS / LINE_SAMPLE_INTERVAL_MS + 1);
    expectMotor(MOTOR_ENCODER_TURN_SPEED, -MOTOR_ENCODER_TURN_SPEED);
    encoderTestCounts = {turnStart.left + 9, turnStart.right - 8};
    hold(0);
    expectMotor(MOTOR_MIN_SPEED, -MOTOR_MIN_SPEED);
    encoderTestCounts = {turnStart.left + 10, turnStart.right - 10};
    hold(0);
    expectMotor(0, 0);
    hold(0, MOTOR_RAMP_MS / LINE_SAMPLE_INTERVAL_MS);
    expectMotor(-motorBaseSpeed, -motorBaseSpeed);

    // Truong hop mat tin hieu: KHONG duoc dong vinh vien (loi D5). Robot phai
    // dung an toan, in canh bao va TIEP TUC phan hoi cam bien ngay sau do.
    approachEncoderTurn();
    hold(0, ENCODER_TURN_TIMEOUT_MS / LINE_SAMPLE_INTERVAL_MS);
    expectMotor(0, 0);
    // Sau khi that bai, robot phuc hoi va tiep tuc chay theo duong.
    hold(straight, 30);
    expectMotor(motorBaseSpeed, motorBaseSpeed);

    startTracking();
    encoderTestCounts = {3, 5};
    toggleLineActiveState();
    hold(straight);
    assert(encoderTestCounts.left == 3 && encoderTestCounts.right == 5);
    updateLineTracking(false);
    encoderTestEnabled = false;
}
#endif

#if NUM_SENSORS == 8
// ------------------------------------------------------------------------------
// PHAN LOAI NGA RE 90 DO (pure classifier)
// ------------------------------------------------------------------------------
static void testJunctionClassifier() {
    // Cac mau nga re TRAI bao cao trong de bai.
    assert(detectJunction(0x07) == JUNCTION_LEFT);  // "###....."
    assert(detectJunction(0x0F) == JUNCTION_LEFT);  // "####...."
    assert(detectJunction(0x17) == JUNCTION_LEFT);  // "###.#..."
    // Cac mau nga re PHAI bao cao trong de bai.
    assert(detectJunction(0xE0) == JUNCTION_RIGHT); // ".....###"
    assert(detectJunction(0xF0) == JUNCTION_RIGHT); // "....####"

    // Lech nhe va duong chinh con nam duoi xe -> KHONG phai nga re 90 do.
    assert(detectJunction(0x01) == JUNCTION_NONE);  // "#......."
    assert(detectJunction(0x02) == JUNCTION_NONE);  // ".#......"
    assert(detectJunction(0x80) == JUNCTION_NONE);  // ".......#"
    assert(detectJunction(0x18) == JUNCTION_NONE);  // "...##..." (duong chinh)
    assert(detectJunction(0x19) == JUNCTION_TEE_LEFT);
    assert(detectJunction(0x1F) == JUNCTION_TEE_LEFT);  // "#####..."
    assert(detectJunction(0xF8) == JUNCTION_TEE_RIGHT);
    assert(detectJunction(0x3C) == JUNCTION_CROSS);
    assert(detectJunction(0xFF) == JUNCTION_CROSS);
    assert(detectJunction(0xBB) == JUNCTION_CROSS); // "##.###.#"
    assert(detectJunction(0x00) == JUNCTION_NONE);  // mat vach

    // Dem cam bien den.
    assert(countBlackSensors(0x07) == 3);
    assert(countBlackSensors(0xFF) == 8);
    assert(countBlackSensors(0x00) == 0);

    // Vach ngang (tram servo) cat ngang qua giua thanh cam bien.
    assert(isCrossPattern(0xFF));
    assert(isCrossPattern(0x3C)); // 4 cam bien giua S3..S6
    assert(isCrossPattern(0x7C)); // 5 cam bien
    assert(isCrossPattern(0x7F)); // 7 cam bien den (S1..S7)
    assert(!isCrossPattern(0x18)); // duong chinh (S4+S5)
    assert(!isCrossPattern(0x07)); // nga re trai
    assert(!isCrossPattern(0xE0)); // nga re phai
    assert(!isCrossPattern(0x00)); // mat vach
}

// ------------------------------------------------------------------------------
// NGA RE 90 DO: XAC NHAN 2 MAU + QUAY TAI CHO BANG ENCODER
// ------------------------------------------------------------------------------
#ifdef SENSOR_PHOTORESISTOR
// Dua robot vao "nhanh trai" 90 do: "###....." (0x07).
static void testJunctionLeftPivot() {
    encoderTestEnabled = true;
    encoderTestCounts = {0, 0};


    startTracking();
    holdConfirmedStraight();
    motorEvents.clear();

    // Mau 1: chi ghi nhan, chua duoc quay (can JUNCTION_CONFIRM_SAMPLES mau).
    hold(0x07);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    // Mau 2 xac nhan nga re. Chi quay sau khi tam xe toi nga re.
    hold(0x07);
    advanceToJunctionCenter();
    expectMotor(-MOTOR_MAX_SPEED, MOTOR_MAX_SPEED);
    const auto turnStart = encoderTestCounts;

    // Quay du goc: 20 xung lech (turnPulses(90) == 20).
    // Con 6 xung: van quay nhanh. Vung giam toc: <= turnPulses(20 deg) = 5 xung.
    encoderTestCounts = {turnStart.left - 7, turnStart.right + 7};
    hold(0x07);
    expectMotor(-MOTOR_MAX_SPEED, MOTOR_MAX_SPEED);
    // leftDelta = -8, rightDelta = +9 -> difference = 17 -> remaining = 3.
    encoderTestCounts = {turnStart.left - 8, turnStart.right + 9};
    hold(0x07);
    expectMotor(-JUNCTION_FINE_TURN_SPEED, JUNCTION_FINE_TURN_SPEED);
    // remaining = 0 -> dung tai cho.
    encoderTestCounts = {turnStart.left - 10, turnStart.right + 10};
    hold(0x07);
    expectMotor(0, 0);

    // Quan tinh vuot goc: vach giua xuat hien nhung xe phai sua lai goc.
    encoderTestCounts = {turnStart.left - 13, turnStart.right + 13};
    holdFor(straight, JUNCTION_POST_PIVOT_SETTLE_MS);
    expectMotor(JUNCTION_FINE_TURN_SPEED, -JUNCTION_FINE_TURN_SPEED);
    hold(straight, 2);
    expectMotor(JUNCTION_FINE_TURN_SPEED, -JUNCTION_FINE_TURN_SPEED);
    encoderTestCounts = {turnStart.left - 10, turnStart.right + 10};
    hold(straight, 2);
    expectMotor(motorBaseSpeed, motorBaseSpeed);

    encoderTestEnabled = false;
}

// ------------------------------------------------------------------------------
// NGA RE PHAI 90 DO (".....###" = 0xE0) + GIAO LO NGANG
// ------------------------------------------------------------------------------
static void testJunctionRightPivot() {
    encoderTestEnabled = true;
    startTracking();

    // Vach ngang xuat phat khong duoc quay hoac khoi dong tram.
    hold(cross, 3);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    assert(servoEvents.empty());
    for (unsigned long time = 0; time <= CROSS_FORWARD_CONFIRM_MS; time += 3 * LINE_SAMPLE_INTERVAL_MS) {
        hold(0x08);
        hold(0);
        hold(0x10);
    }
    hold(straight, 2);
    // Nga T cat ngang ca thanh cam bien chia se mau voi dau +.
    completeRouteTurn(0xBB, 0xFF, 1);

    // Mau T con sot lai khong duoc nhan thanh giao lo tiep theo.
    hold(0x1F, 3);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(straight, 2);
    // Hai giao lo co the gan nhau. Vach ngang va cum phai deu xac nhan cung lenh.
    completeRouteTurn(0x3C, 0x98, -1);
    assert(servoEvents.empty());
    encoderTestEnabled = false;
}

// ------------------------------------------------------------------------------
// PHOI HOI SAU KHI QUAY THAT BAI (khong duoc dong vinh vien)
// ------------------------------------------------------------------------------
static void testJunctionPivotFailureRecovers() {
    encoderTestEnabled = true;
    encoderTestCounts = {0, 0};


    startTracking();
    holdConfirmedStraight();

    hold(0x07);
    hold(0x07);
    advanceToJunctionCenter();
    expectMotor(-MOTOR_MAX_SPEED, MOTOR_MAX_SPEED);

    // Mat tin hieu encoder: quay khong bao gio ket thuc -> het timeout.
    // Sau khi het timeout, robot dung, roi xoay tai cho sang phia vach vua
    // mat de tim lai duong (khong dung hanh) - KHONG brick.
    hold(0, JUNCTION_PIVOT_TIMEOUT_MS / LINE_SAMPLE_INTERVAL_MS + 2);
    // Robot PHAI phuc hoi va tiep tuc phan hoi cam bien (khong brick).
    hold(0, 2);
    expectMotor(-MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
    // Tim thay vach -> phai quay lai chay duong binh thuong.
    hold(straight, 3);
    expectMotor(motorBaseSpeed, motorBaseSpeed);

    holdFor(straight, JUNCTION_COOLDOWN_MS);
    completeRouteTurn(0x3C, 0xFF, 1);
    encoderTestEnabled = false;
}
#endif  // SENSOR_PHOTORESISTOR

// ------------------------------------------------------------------------------
// SPEED PI: PHAI GIU NGUYEN LENH KHI KHONG CO DU lieu VAN TOC DO HOP LE
// ------------------------------------------------------------------------------
static void testSpeedPidHoldsCommand() {
    // Chua du chu ky PI -> giu nguyen lenh goc.
    SpeedPidState state{};
    speedPidReset(state);
    assert(speedPidCorrect(state, 230.0f, 0.0f, 687, 1000) == 687);
    // Chua co mau van toc do hop le (NaN) -> giu nguyen lenh goc.
    assert(speedPidCorrect(state, 230.0f, NAN, 687, 2000) == 687);

    const auto corrected = speedPidCorrect(state, 230.0f, 180.0f, 687, 2020);
    assert(corrected > 687);
    assert(speedPidCorrect(state, 230.0f, 180.0f, 687, 2025) == corrected);
    // Van toc do do duoc = 0 trong khi lenh chay -> bao mat encoder sau
    // SPEED_PID_STALL_WINDOWS cua so lien tiep.
    for (int i = 0; i < SPEED_PID_STALL_WINDOWS + 1; ++i) {
        const unsigned long t = 2000UL + (i + 1) * SPEED_PID_PERIOD_MS;
        speedPidCorrect(state, 230.0f, 0.0f, 687, t);
    }
    assert(state.stalled);
    // Reset xoa trang thai stall.
    speedPidReset(state);
    assert(!state.stalled);
}

// ------------------------------------------------------------------------------
// VACH NGANG 5-7 CAM BIEN DEN PHAI KICH HOAT TRAM SERVO
// ------------------------------------------------------------------------------
static void testCrossPatternArmsStation() {
    startTracking();
    setCrossReverseDuration(100);
    // Chay thang de "vuot qua" moc 140 ms -> tram servo duoc trang bi.
    holdConfirmedStraight();
    // Vach ngang den 7 cam bien (0x7F = 01111111) - KHONG phai ca 8 cam bien.
    // Truoc day mau nay bi phan loai sai thanh LEFT/FORWARD nen chuoi quay
    // tai cho o tram servo khong bao gio duoc kich hoat.
#ifdef SENSOR_PHOTORESISTOR
    preparePhotoresStations();
#endif
    hold(0x7F, 5);
    holdConfirmedStraight();
    // Cho ca chuoi quay tai cho chay het (buoc servo o giua chuoi).
    holdFor(straight, 6000);
    assert(!servoEvents.empty());
    // Tra lai tham so goc cho cac test sau.
    setCrossReverseDuration(CROSS_REVERSE_DEFAULT_MS);
}
#endif

static void testAllMasks() {
    for (unsigned int raw = 0; raw <= cross; ++raw) {
        LineAction expected = LINE_STOP;
        if (raw == cross) expected = LINE_CROSS;
        else if ((raw & 0x0F) && (raw & 0xF0)) expected = LINE_FORWARD;
        else if (raw & 0x0F) expected = LINE_LEFT;
        else if (raw & 0xF0) expected = LINE_RIGHT;
        assert(getLineAction(raw) == expected);
    }
}

static void testSteeringTracksCentroid() {
    startTracking();
    hold(0x1C, 2); // S3..S5: line lech trai, ca hai nua van thay vach.
    assert(left > 0 && left < right && right <= MOTOR_MAX_SPEED);
    const int innerSpeed = left;
    const int outerSpeed = right;

    hold(0);
#ifdef SENSOR_PHOTORESISTOR
    expectMotor(innerSpeed, outerSpeed);
    hold(0);
    expectMotor(-MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
#else
    expectMotor(0, 0);
#endif
    hold(0x38, 2); // S4..S6: lech phai cung khoang cach.
    expectMotor(outerSpeed, innerSpeed);

    hold(0x3E, 2); // Vach rong: chay deu hai ben, xoa sai lech PD.
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(straight, 2);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
}

#ifndef SENSOR_PHOTORESISTOR
static void testTurnRecovery() {
    startTracking();
    hold(leftOnly, 3);
    assert(left >= 0 && left < motorBaseSpeed && right > motorBaseSpeed && right <= MOTOR_MAX_SPEED);
    const int innerSpeed = left;
    const int outerSpeed = right;
    hold(0);
    expectMotor(innerSpeed, outerSpeed);
    hold(leftOnly, 2);
    expectMotor(innerSpeed, outerSpeed);
    hold(0, 2);
    expectMotor(-innerSpeed, -outerSpeed);
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
    expectMotor(outerSpeed, innerSpeed);
    hold(0, 2);
    expectMotor(-outerSpeed, -innerSpeed);
    updateLineTracking(false);
    expectMotor(0, 0);
    hold(0, 25);
    expectMotor(0, 0);
}

static void testRecoveryAfterStraight() {
    startTracking();
    hold(leftOnly, 4);
    const int turnLeft = left;
    const int turnRight = right;
    const auto turnStart = motorEvents.front().at;
    hold(straight, 2);
    const auto straightStart = now;
    hold(straight, 3);
    const int straightLeft = left;
    const int straightRight = right;
    hold(0, 2);
    expectMotor(-straightLeft, -straightRight);
    const auto straightDuration = now - straightStart;
    const auto turnDuration = straightStart - turnStart;
    hold(0, straightDuration / LINE_SAMPLE_INTERVAL_MS);
    expectMotor(-turnLeft, -turnRight);
    hold(leftOnly, turnDuration / LINE_SAMPLE_INTERVAL_MS);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    holdFor(cross, 500);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    holdFor(straight, 200);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    assert(servoEvents.empty());

    startTracking();
    holdConfirmedStraight();
    holdFor(cross, 300);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(0, 2);
    expectMotor(-motorBaseSpeed, -motorBaseSpeed);
    holdFor(0, 310);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    holdFor(straight, 200);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    assert(servoEvents.empty());

    startTracking();
    hold(rightOnly, 3);
    holdFor(straight, 200);
    hold(0);
    expectMotor(0, 0);
}
#endif

static void testCrossSequence() {
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
    holdFor(straight, 200);

#ifdef SENSOR_PHOTORESISTOR
    preparePhotoresStations();
#endif
    assert(getCrossReverseDuration() == CROSS_REVERSE_DEFAULT_MS);
    constexpr uint32_t configuredDuration = 750;
    setCrossReverseDuration(configuredDuration);
    holdFor(cross, 300);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    holdFor(straight, 100);
    hold(cross);
    hold(straight, CROSS_FORWARD_CONFIRM_MS / LINE_SAMPLE_INTERVAL_MS + 1);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    motorEvents.clear();
    hold(straight);
    expectMotor(motorBaseSpeed, -motorBaseSpeed);
    const auto startedAt = now;
    setCrossReverseDuration(2500);
    holdFor(0, 6 * CROSS_TURN_45_MS + 6 * MOTOR_RAMP_MS + 4 * CROSS_SERVO_SETTLE_MS + 2 * configuredDuration);
    expectMotor(0, 0);

    const int speed = motorBaseSpeed;
    const MotorEvent expected[] = {
        {0, speed, -speed},
        {2 * CROSS_TURN_45_MS, 0, 0},
        {MOTOR_RAMP_MS, -speed, -speed},
        {configuredDuration, 0, 0},
        {MOTOR_RAMP_MS + 2 * CROSS_SERVO_SETTLE_MS, speed, -speed},
        {CROSS_TURN_45_MS, 0, 0},
        {MOTOR_RAMP_MS + 2 * CROSS_SERVO_SETTLE_MS, -speed, speed},
        {CROSS_TURN_45_MS, 0, 0},
        {MOTOR_RAMP_MS, speed, speed},
        {configuredDuration, 0, 0},
        {MOTOR_RAMP_MS, -speed, speed},
        {2 * CROSS_TURN_45_MS, 0, 0},
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
    holdConfirmedStraight();
    expectMotor(speed, speed);

    hold(cross, 2);
    holdConfirmedStraight();
    expectMotor(speed, -speed);
    lineActiveState = !lineActiveState;
    hold(0);
    expectMotor(0, 0);
    hold(0, 30);
#ifdef SENSOR_PHOTORESISTOR
    expectMotor(-MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
#else
    expectMotor(0, 0);
#endif
}

static void testStartBar() {
    startTracking();
    holdFor(cross, 300);
    hold(0, 2);
#ifdef SENSOR_PHOTORESISTOR
    expectMotor(-MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
#else
    expectMotor(0, 0);
#endif
    motorEvents.clear();
    hold(straight, 5);
    hold(cross, 2);
    holdConfirmedStraight();
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    assert(servoEvents.empty());
    for (const auto& event : motorEvents) {
        assert(event.left >= 0 && event.right >= 0);
    }

    // Sau khi roi start, mat vach sau giao lo dung phuong an tim cua tung ban.
    hold(cross, 3);
    hold(0, 2);
#ifdef SENSOR_PHOTORESISTOR
    expectMotor(-MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
#else
    expectMotor(-motorBaseSpeed, -motorBaseSpeed);
#endif
}

#ifndef SENSOR_PHOTORESISTOR
static void testFirstBranchRetrace() {
    startTracking();
    holdConfirmedStraight();
    motorEvents.clear();

    // Nhan ca nhanh trai va line giua tai nga re.
    hold(0x1F, 2);
    expectMotor(0, motorBaseSpeed);
    hold(0x1F, 3);
    hold(0, 3); // Mat line tam thoi trong luc quay vao nhanh.
    expectMotor(0, motorBaseSpeed);
    hold(leftOnly, 2);
    hold(straight, 2);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
#ifdef SENSOR_PHOTORESISTOR
    hold(0x02, 2);
    expectMotor(MOTOR_MIN_SPEED, MOTOR_MIN_SPEED + MOTOR_EDGE_SPEED_DELTA);
    hold(0x40, 2);
    expectMotor(MOTOR_MIN_SPEED + MOTOR_EDGE_SPEED_DELTA, MOTOR_MIN_SPEED);
#endif
    holdFor(straight, 400); // Di trong nhanh lau hon moc 140 ms cu.

    motorBaseSpeed -= MOTOR_SPEED_STEP;
    holdFor(straight, 200);
    holdFor(cross, 300); // Vach ngang o cuoi nhanh khong phai tram servo.
    holdConfirmedStraight();
    assert(servoEvents.empty());

    const auto outward = motorEvents;
    hold(0, 2);
    const unsigned long lostAt = now;
    for (size_t index = outward.size(); index > 0; --index) {
        const auto& segment = outward[index - 1];
        expectMotor(-segment.left, -segment.right);
        const auto endedAt = index == outward.size() ? lostAt : outward[index].at;
        hold(0, (endedAt - segment.at) / LINE_SAMPLE_INTERVAL_MS);
    }
    expectMotor(motorBaseSpeed, motorBaseSpeed);
#ifdef SENSOR_PHOTORESISTOR
    holdFor(0x02, LINE_RECOVERY_FORWARD_MS);
    expectMotor(MOTOR_MIN_SPEED, MOTOR_MIN_SPEED + MOTOR_EDGE_SPEED_DELTA);
#else
    hold(0x1F, 5);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
#endif
    holdFor(straight, 250);
    assert(servoEvents.empty());
    hold(0x1F, 2);
    expectMotor(motorBaseSpeed, motorBaseSpeed);

    // Ket thuc nhanh cut, cac tram servo phia sau van duoc nhan.
    motorBaseSpeed = MOTOR_BASE_SPEED;
    holdFor(straight, 250);
    hold(cross, 3);
    holdConfirmedStraight();
    expectMotor(motorBaseSpeed, -motorBaseSpeed);
    updateLineTracking(false);
    expectMotor(0, 0);
}

static void testFirstBranchTimeoutAndCancel() {
    startTracking();
    holdConfirmedStraight();
    // 0x1F = S1..S5 den: duong chinh con nam duoi xe -> NHANH 3 GIA (nhanh
    // trai), khong phai nga re 90 do. Nga re 90 do la 0x0F (S5 trang).
    hold(0x1F);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(straight);
    hold(0x1F, 2);
    expectMotor(0, motorBaseSpeed);
    hold(0, FIRST_BRANCH_TURN_TIMEOUT_MS / LINE_SAMPLE_INTERVAL_MS - 1);
    expectMotor(0, motorBaseSpeed);
    hold(0);
    expectMotor(0, -motorBaseSpeed);
    hold(0, FIRST_BRANCH_TURN_TIMEOUT_MS / LINE_SAMPLE_INTERVAL_MS);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(0, LINE_RECOVERY_FORWARD_MS / LINE_SAMPLE_INTERVAL_MS);
#ifdef SENSOR_PHOTORESISTOR
    expectMotor(-MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
#else
    expectMotor(0, 0);
#endif

    startTracking();
    holdConfirmedStraight();
    hold(0x1F, 2);
    hold(straight, 2);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    hold(0, 2);
    expectMotor(-motorBaseSpeed, -motorBaseSpeed);
    updateLineTracking(false);
    expectMotor(0, 0);
    hold(0, 20);
#ifdef SENSOR_PHOTORESISTOR
    expectMotor(-MOTOR_MIN_SPEED, MOTOR_MIN_SPEED);
#else
    expectMotor(0, 0);
#endif
    holdConfirmedStraight();
    hold(0x1F, 2);
    expectMotor(0, motorBaseSpeed);
}

static void testBranchHistoryCapacity() {
    startTracking();
    holdConfirmedStraight();
    motorEvents.clear();
    hold(0x1F, 2);
    hold(straight, 2);
    for (unsigned int i = 0; i < 128 && left >= 0 && right >= 0; ++i) {
        hold(i % 2 == 0 ? leftOnly : rightOnly, 2);
    }
    assert(left < 0 || right < 0);
    const auto returnedAt = now;
    auto outward = motorEvents;
    outward.pop_back(); // Bo lenh lui dau tien.
    assert(outward.size() == 128);
    for (size_t index = outward.size(); index > 0; --index) {
        const auto& segment = outward[index - 1];
        expectMotor(-segment.left, -segment.right);
        const auto endedAt = index == outward.size() ? returnedAt : outward[index].at;
        hold(0, (endedAt - segment.at) / LINE_SAMPLE_INTERVAL_MS);
    }
    expectMotor(motorBaseSpeed, motorBaseSpeed);
}

#endif

static void testThreeServoStations() {
    startTracking();
    setCrossReverseDuration(100);
    hold(cross, 5);
    holdConfirmedStraight();

    // Goc re chi thay mot ben se khong tinh thanh tram servo.
    hold(leftOnly, 3);
    holdFor(straight, 250);
    hold(rightOnly, 3);
    holdFor(straight, 250);
    assert(servoEvents.empty());

#ifdef SENSOR_PHOTORESISTOR
    preparePhotoresStations();
#endif
    for (unsigned int station = 0; station < 3; ++station) {
        hold(cross, 5);
        holdConfirmedStraight();
        expectMotor(motorBaseSpeed, -motorBaseSpeed);
        holdFor(cross, 6000);
        assert(servoEvents.size() == (station + 1) * 4);

        // Doc lai cung vach sau khi quay ve khong lap chuoi.
        hold(straight, 5);
        hold(cross, 5);
        holdConfirmedStraight();
        expectMotor(motorBaseSpeed, motorBaseSpeed);
        assert(servoEvents.size() == (station + 1) * 4);
    }

    hold(cross, 5);
    holdFor(straight, 1000);
    expectMotor(motorBaseSpeed, motorBaseSpeed);
    assert(servoEvents.size() == 12);

    // Het ba tram, co che lui tim line van hoat dong.
    hold(cross, 5);
    hold(0, 2);
#ifdef SENSOR_PHOTORESISTOR
    expectMotor(MOTOR_MIN_SPEED, -MOTOR_MIN_SPEED);
#else
    expectMotor(-motorBaseSpeed, -motorBaseSpeed);
#endif

    // Bat lai che do tu dong tao mot luot chay moi.
    startTracking();
    holdConfirmedStraight();
#ifdef SENSOR_PHOTORESISTOR
    preparePhotoresStations();
#endif
    hold(cross, 5);
    holdConfirmedStraight();
    expectMotor(motorBaseSpeed, -motorBaseSpeed);
}

#endif

int main() {
#ifdef SENSOR_PHOTORESISTOR
    testPhotoresThresholds();
    testPhotoresSampleInterval();
    testPhotoresEdgesAndSearch();
    testEncoderHeadingAndTurn();
#endif
#if NUM_SENSORS == 8
    testJunctionClassifier();
#ifdef SENSOR_PHOTORESISTOR
    testJunctionLeftPivot();
    testJunctionRightPivot();
    testJunctionPivotFailureRecovers();
#endif
    testSpeedPidHoldsCommand();
    testCrossPatternArmsStation();
    testAllMasks();
    testSteeringTracksCentroid();
#ifndef SENSOR_PHOTORESISTOR
    testTurnRecovery();
    testRecoveryAfterStraight();
#endif
    testCrossSequence();
    testStartBar();
    testThreeServoStations();
#ifndef SENSOR_PHOTORESISTOR
    testFirstBranchRetrace();
    testFirstBranchTimeoutAndCancel();
    testBranchHistoryCapacity();
#endif
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
