#include <Arduino.h>
#include "config.h"
#include "motor_driver.h"
#include "line_sensor.h"
#include "servo1.h"
#include "servo2.h"
#include "servo3.h"
#include "ble_controller.h"
#include "line_tracking.h"

struct LineTrackingState {
    unsigned long sampledAt = 0;
    unsigned long lostAt = 0;
    uint8_t previousRaw = 0;
    uint8_t olderRaw = 0;
    uint8_t sampleCount = 0;
    uint8_t mode = EDGE_FOLLOW_MODE;
    uint8_t activeLevel = LINE_ACTIVE_STATE;
    LineEdge lastEdge = {0.0f, EDGE_AUTO};
    float filteredError = 0.0f;
    bool hasPreviousError = false;
    bool sharpTurn = false;
    int searchDirection = 0;
    bool crossing = false;
    unsigned long crossStartedAt = 0;
};

static LineTrackingState tracking;
static unsigned long lastDebugTime = 0;

void setup() {
    Serial.begin(115200);
    delay(1000); // Cho on dinh nguon va ket noi Serial USB

    Serial.println("==================================================");
    Serial.println("   WDF LINE TRACKER: LINE SENSOR + L298N + SERVO  ");
    Serial.println("         BLUEFRUIT BLE WIRELESS CONTROL           ");
    Serial.println("==================================================");

    // 1. Khoi tao phan cung dong co L298N
    motorDriverInit();

    // 2. Khoi tao 8 mat cam bien do line
    lineSensorInit();

    // 3. Khoi tao 3 Servo doc lap
    servo1_init();
    servo2_init();
    servo3_init();

    // 4. Khoi tao Bluetooth Low Energy (Adafruit Bluefruit LE)
    bleControllerInit();

    // Dừng động cơ, chờ người dùng bấm phím 1 trên app
    stopMotion();

    Serial.println("==================================================");
    Serial.println("[System] KHOI DONG THANH CONG!");
    Serial.println("[System] Robot dang o trang thai: CHO (STANDBY).");
    Serial.printf("[System] -> Ket noi Bluetooth '%s' va\n", BLE_DEVICE_NAME);
    Serial.println("[System] -> Bam phim '1' de BAT DAU do line!");
    Serial.println("==================================================");
}

// Chuyen van toc do chuan hoa (-1000..+1000) sang duty PWM (0-255) va nguoc lai.
// He so PID duoc giu nguyen tren thang do PWM de khong phai chinh lai.
static int speedToPwm(int normalizedSpeed) {
    const int magnitude = constrain(abs(normalizedSpeed), 0, MOTOR_SPEED_SCALE);
    return (int)((long)magnitude * 255L / MOTOR_SPEED_SCALE);
}

static int pwmToSpeed(int pwm) {
    return (int)((long)constrain(pwm, -255, 255) * MOTOR_SPEED_SCALE / 255L);
}

static void searchForLine(uint8_t raw, unsigned long now, int baseSpeedPwm) {
    if (tracking.searchDirection == 0) {
        tracking.lostAt = now;
        tracking.hasPreviousError = false;
        tracking.sharpTurn = false;

        const EdgeMode polarity = tracking.mode == EDGE_AUTO ? tracking.lastEdge.polarity : static_cast<EdgeMode>(tracking.mode);
        if (fabsf(tracking.lastEdge.position) >= 1.0f) {
            tracking.searchDirection = tracking.lastEdge.position < 0.0f ? -1 : 1;
        } else if ((polarity == EDGE_WHITE_LEFT_BLACK_RIGHT || polarity == EDGE_BLACK_LEFT_WHITE_RIGHT) &&
                   (raw == 0x00 || raw == LINE_TRACKING_MASK)) {
            const bool whiteOnLeft = polarity == EDGE_WHITE_LEFT_BLACK_RIGHT;
            const bool onWhite = raw == 0x00;
            tracking.searchDirection = (onWhite == whiteOnLeft) ? 1 : -1;
        } else {
            tracking.searchDirection = tracking.lastEdge.position < 0.0f ? -1 : 1;
        }
    }

    // Mau tron chi con mep nguoc chieu: tim ve phia cac mat dang thay den.
    if (raw != 0x00 && raw != LINE_TRACKING_MASK) {
        tracking.searchDirection = (raw & LINE_TRACKING_FIRST_BIT) ? -1 : 1;
    }

    if (now - tracking.lostAt >= LINE_SEARCH_TIMEOUT_MS) {
        stopMotion();
    } else if (tracking.searchDirection < 0) {
        runTurnLeft(pwmToSpeed(min(baseSpeedPwm, MOTOR_SEARCH_SPEED_PWM)));
    } else {
        runTurnRight(pwmToSpeed(min(baseSpeedPwm, MOTOR_SEARCH_SPEED_PWM)));
    }
}

// Ben trong cua vong tron se dung that (0) khi yeu cau PWM nho hon dead-band,
// thay vi bi "keo hai" o duty nho khong duoc chay -> xe om cong sach hon.
static int clampCurveMotorSpeed(int rawSpeed, int minPwm) {
    const int deadband = getMotorRuntimeConfig().deadbandLeft;
    if (rawSpeed < deadband) {
        return 0;
    }
    return constrain(rawSpeed, minPwm, MOTOR_MAX_SPEED_PWM);
}

// Lam tron duty theo buoc 5/255 de tranh PWM nhieu lien tuc gay tieng keo.
static int quantizePwm(int pwm) {
    return (pwm / 5) * 5;
}

static void processLegacyLineTracking() {
    const uint8_t mode = edgeFollowMode;
    const uint8_t activeLevel = lineActiveState;
    if (tracking.mode != mode || tracking.activeLevel != activeLevel) {
        tracking = {};
        tracking.mode = mode;
        tracking.activeLevel = activeLevel;
        stopMotion();
    }

    const unsigned long now = millis();
    const unsigned long elapsed = now - tracking.sampledAt;
    if (elapsed < PID_SAMPLE_INTERVAL_MS) {
        return;
    }
    tracking.sampledAt = now;

    const uint8_t raw = readRawSensors();
#if NUM_SENSORS == 8 && !defined(SENSOR_PHOTORESISTOR)
    // Hai mau lien tiep phai cung toan black, tranh ghep 8 bit tu cac mau khac nhau thanh cross gia.
    const bool allBlack = isCrossLinePattern(raw) && isCrossLinePattern(tracking.previousRaw);
#endif
    // Moi bit can xuat hien trong it nhat hai trong ba mau.
    const uint8_t filteredRaw = (raw & tracking.previousRaw) |
                                (raw & tracking.olderRaw) |
                                (tracking.previousRaw & tracking.olderRaw);
    tracking.olderRaw = tracking.previousRaw;
    tracking.previousRaw = raw;
    if (tracking.sampleCount < 2) {
        ++tracking.sampleCount;
        return;
    }

    const int baseSpeedPwm = motorBaseSpeed > 0 ? speedToPwm(motorBaseSpeed) : MOTOR_BASE_SPEED_PWM;
    const uint8_t lineRaw = filteredRaw & LINE_TRACKING_MASK;
#if NUM_SENSORS == 8 && !defined(SENSOR_PHOTORESISTOR)
    if (allBlack && !tracking.crossing) {
        tracking.crossing = true;
        tracking.crossStartedAt = now;
        tracking.hasPreviousError = false;
        tracking.sharpTurn = false;
        tracking.searchDirection = 0;
    }
    if (tracking.crossing) {
        // Chi di thang mot nhip co gioi han. Khong khoi dong lai nhip khi cross van con.
        if (now - tracking.crossStartedAt < CROSS_LINE_PASS_DELAY_MS) {
            runForward(pwmToSpeed(min(baseSpeedPwm, MOTOR_BASE_SPEED_PWM)));
            return;
        }
        if (allBlack) {
            stopMotion();
            return;
        }
    }
#endif

    // Khong dieu khien theo mau da phan loai mo ho; -#-..-#- la ngoai le can tam hop le.
    if (isLineSampleAmbiguous(lineRaw)) {
        stopMotion();
        tracking.hasPreviousError = false;
        tracking.sharpTurn = false;
        tracking.searchDirection = 0;
        return;
    }

    LineEdge edge;
    if (!findLineEdge(lineRaw, mode, tracking.lastEdge.position, edge)) {
        searchForLine(lineRaw, now, baseSpeedPwm);
        return;
    }

    tracking.crossing = false;
    tracking.searchDirection = 0;
    tracking.lastEdge = edge;

    float derivative = 0.0f;
    if (tracking.hasPreviousError) {
        const float change = LINE_ERROR_FILTER_ALPHA * (edge.position - tracking.filteredError);
        tracking.filteredError += change;
        derivative = change * PID_SAMPLE_INTERVAL_MS / elapsed;
    } else {
        tracking.filteredError = edge.position;
        tracking.hasPreviousError = true;
    }

    const float error = tracking.filteredError;
    const float turnThreshold = tracking.sharpTurn ? SHARP_TURN_EXIT_THRESHOLD : SHARP_TURN_THRESHOLD;
    tracking.sharpTurn = fabsf(error) >= turnThreshold && fabsf(edge.position) > SHARP_TURN_EXIT_THRESHOLD &&
                         error * edge.position > 0.0f;
    if (tracking.sharpTurn) {
        const int speedPwm = min(baseSpeedPwm, MOTOR_SPIN_SPEED_PWM);
        if (error < 0.0f) runTurnLeft(pwmToSpeed(speedPwm));
        else runTurnRight(pwmToSpeed(speedPwm));
        return;
    }

    const int cruiseSpeed = max(MOTOR_MIN_SPEED_PWM,
                                baseSpeedPwm - static_cast<int>(LINE_CURVE_SPEED_DROP * fabsf(error)));
    const float correction = PID_KP * error + PID_KD * derivative;

    // Hieu chinh theo he van toc do chuan hoa: ben trai tang, ben phai giam
    // khi xe lech sang phai (error > 0) - luon o frame robot, khong phu thuoc
    // INVERT_*/SWAP_* (do cuc thuoc tinh duoc apply o tang driver).
    const int leftSpeed = clampCurveMotorSpeed(
        static_cast<int>(cruiseSpeed + correction), MOTOR_MIN_PWM_LEFT);
    const int rightSpeed = clampCurveMotorSpeed(
        static_cast<int>(cruiseSpeed - correction), MOTOR_MIN_PWM_RIGHT);

    runLR(pwmToSpeed(quantizePwm(leftSpeed)), pwmToSpeed(quantizePwm(rightSpeed)));
}

static void processLineTracking() {
#if NUM_SENSORS == 8 && !defined(SENSOR_PHOTORESISTOR)
    const unsigned long now = millis();
    if (tracking.activeLevel != lineActiveState) {
        tracking.activeLevel = lineActiveState;
        resetLineTracking();
        stopMotion();
        return;
    }
    if (now - tracking.sampledAt < LINE_SAMPLE_INTERVAL_MS) return;
    const unsigned long sampleGap = tracking.sampledAt == 0 ? LINE_SAMPLE_INTERVAL_MS : now - tracking.sampledAt;
    tracking.sampledAt = now;
    const uint8_t raw = readRawSensors();
    updateLineTracking(raw, now, speedToPwm(motorBaseSpeed));
    const int requestedLeft = getLineLeftPwm(), requestedRight = getLineRightPwm();
    runLR(pwmToSpeed(requestedLeft), pwmToSpeed(requestedRight), MOTOR_OUTPUT_PERCENT, true);

    // Cung mot mau: input -> quyet dinh -> duty LEDC, khong ghep cac ban tin
    // sensor/motor doc o nhung thoi diem khac nhau de suy luan.
    static unsigned long loggedAt = 0, maxGap = 0;
    static const char* loggedState = nullptr;
    maxGap = max(maxGap, sampleGap);
    const char* state = getLineTrackingStatus();
    if (state != loggedState || now - loggedAt >= 250) {
        char rawPattern[9], filteredPattern[9];
        const uint8_t filtered = getLineFilteredRaw();
        for (int i = 0; i < 8; ++i) {
            rawPattern[i] = (raw & (1U << i)) ? '#' : '.';
            filteredPattern[i] = (filtered & (1U << i)) ? '#' : '.';
        }
        rawPattern[8] = filteredPattern[8] = '\0';
        int outputLeft, outputRight;
        // LEDC chi ap duty moi o chu ky PWM ke tiep (100 us o 10 kHz).
        // Chi luc ghi log moi cho mot chu ky, tranh doc nham duty cu thanh loi driver.
        delayMicroseconds((1000000UL + PWM_FREQ - 1) / PWM_FREQ + 1);
        getMotorOutputPwm(outputLeft, outputRight);
        Serial.printf("[AutoLine] Raw:[%s] Filter:[%s] Track:%s ReqPWM L=%d R=%d OutPWM L=%d R=%d GapMax=%lums\n",
                      rawPattern, filteredPattern, state, requestedLeft, requestedRight,
                      outputLeft, outputRight, maxGap);
        loggedAt = now;
        loggedState = state;
        maxGap = 0;
    }
#else
    // Cac combo khac giu thuat toan cua recovery trong thu nghiem nay.
    processLegacyLineTracking();
#endif
}

void loop() {
    motorDriverUpdate();
    bleControllerUpdate();

    // 2. Cap nhat cac task Servo chay rieng biet
    servo1_update();
    servo2_update();
    servo3_update();

    // 3. Xu ly di chuyen tuy theo Che do
    static bool wasTracking = false;
    const bool isTracking = currentRobotMode == MODE_LINE_TRACKING;
    if (isTracking) {
        if (!wasTracking) {
            tracking = {};
            resetLineTracking();
            stopMotion();
        }
        processLineTracking();
    }
    wasTracking = isTracking;

    // 4. Gui trang thai 8 mat sensor ve BLE UI (UART Terminal tab) dinh ky moi 300ms
    unsigned long now = millis();
    static unsigned long lastBleTelemetryTime = 0;
    if (isBleConnected() && bleTelemetryStreamEnabled) {
        if (now - lastBleTelemetryTime >= 300) {
            lastBleTelemetryTime = now;
            bleSendLog(getSensorTelemetryCompact());
        }
    }

    // 5. In debug ra Serial dinh ky moi 500ms
    if (now - lastDebugTime >= 500) {
        lastDebugTime = now;
        printSensorDebug();
    }
}
