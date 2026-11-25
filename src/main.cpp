#include <Arduino.h>
#include "config.h"
#include "motor_driver.h"
#include "line_sensor.h"
#include "servo1.h"
#include "servo2.h"
#include "servo3.h"
#include "ble_controller.h"

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
};

static LineTrackingState tracking;
static unsigned long lastDebugTime = 0;

void setup() {
    Serial.begin(115200);
    delay(1000); // Cho on dinh nguon va ket noi Serial USB

    Serial.println("==================================================");
    Serial.println("   ESP32-S3 ROBOT: 8 SENSORS + L298N + 3 SERVO   ");
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
    stopMotors();

    Serial.println("==================================================");
    Serial.println("[System] KHOI DONG THANH CONG!");
    Serial.println("[System] Robot dang o trang thai: CHO (STANDBY).");
    Serial.println("[System] -> Ket noi Bluetooth 'xe do line' va");
    Serial.println("[System] -> Bam phim '1' de BAT DAU bam vien line!");
    Serial.println("==================================================");
}

static void searchForLine(uint8_t raw, unsigned long now, int baseSpeed) {
    if (tracking.searchDirection == 0) {
        tracking.lostAt = now;
        tracking.hasPreviousError = false;
        tracking.sharpTurn = false;

        const EdgeMode polarity = tracking.mode == EDGE_AUTO ? tracking.lastEdge.polarity : static_cast<EdgeMode>(tracking.mode);
        if ((polarity == EDGE_WHITE_LEFT_BLACK_RIGHT || polarity == EDGE_BLACK_LEFT_WHITE_RIGHT) &&
            (raw == 0x00 || raw == SENSOR_MASK)) {
            const bool whiteOnLeft = polarity == EDGE_WHITE_LEFT_BLACK_RIGHT;
            const bool onWhite = raw == 0x00;
            tracking.searchDirection = (onWhite == whiteOnLeft) ? 1 : -1;
        } else {
            tracking.searchDirection = tracking.lastEdge.position < 0.0f ? -1 : 1;
        }
    }

    // Mau tron chi con mep nguoc chieu: tim ve phia cac mat dang thay den.
    if (raw != 0x00 && raw != SENSOR_MASK) {
        tracking.searchDirection = (raw & 0x01) ? -1 : 1;
    }

    if (now - tracking.lostAt >= LINE_SEARCH_TIMEOUT_MS) {
        stopMotors();
    } else if (tracking.searchDirection < 0) {
        spinLeft(min(baseSpeed, MOTOR_SEARCH_SPEED));
    } else {
        spinRight(min(baseSpeed, MOTOR_SEARCH_SPEED));
    }
}

static void processLineTracking() {
    const uint8_t mode = edgeFollowMode;
    const uint8_t activeLevel = lineActiveState;
    if (tracking.mode != mode || tracking.activeLevel != activeLevel) {
        tracking = {};
        tracking.mode = mode;
        tracking.activeLevel = activeLevel;
        stopMotors();
    }

    const unsigned long now = millis();
    const unsigned long elapsed = now - tracking.sampledAt;
    if (elapsed < PID_SAMPLE_INTERVAL_MS) {
        return;
    }
    tracking.sampledAt = now;

    const uint8_t raw = readRawSensors();
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

    const int baseSpeed = motorBaseSpeed;
    LineEdge edge;
    if (!findLineEdge(filteredRaw, mode, tracking.lastEdge.position, edge)) {
        searchForLine(filteredRaw, now, baseSpeed);
        return;
    }

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
        const int speed = min(baseSpeed, MOTOR_SPIN_SPEED);
        if (error < 0.0f) spinLeft(speed);
        else spinRight(speed);
        return;
    }

    const int cruiseSpeed = max(MOTOR_MIN_SPEED, baseSpeed - static_cast<int>(LINE_CURVE_SPEED_DROP * fabsf(error)));
    const float correction = PID_KP * error + PID_KD * derivative;
    const int leftSpeed = constrain(static_cast<int>(cruiseSpeed + correction), MOTOR_MIN_PWM_LEFT, MOTOR_MAX_SPEED);
    const int rightSpeed = constrain(static_cast<int>(cruiseSpeed - correction), MOTOR_MIN_PWM_RIGHT, MOTOR_MAX_SPEED);

    setMotorSpeeds(leftSpeed, rightSpeed);
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
            stopMotors();
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
