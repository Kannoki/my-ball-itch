#include <Arduino.h>
#include "config.h"
#include "motor_driver.h"
#include "line_sensor.h"
#include "servo1.h"
#include "servo2.h"
#include "servo3.h"
#include "ble_controller.h"

// Chi nho lan doc truoc va thoi diem bat dau qua nga tu.
struct LineTrackingState {
    unsigned long sampledAt = 0;
    LineAction previousAction = LINE_STOP;
    uint8_t activeLevel = LINE_ACTIVE_STATE;
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

// Doc line moi 10 ms. Cung mot ket luan hai lan lien tiep moi doi huong.
static void processLineTracking() {
    if (tracking.activeLevel != lineActiveState) {
        tracking = {};
        tracking.activeLevel = lineActiveState;
        stopMotion();
        return;
    }

    const unsigned long now = millis();
    if (now - tracking.sampledAt < LINE_SAMPLE_INTERVAL_MS) return;
    tracking.sampledAt = now;

    const LineAction action = getLineAction(readRawSensors());
    // Het gio qua cross thi dung, ke ca khi mau moi chua du hai lan xac nhan.
    if (tracking.crossing && now - tracking.crossStartedAt >= CROSS_LINE_PASS_DELAY_MS) {
        stopMotion();
    }

    // Khong thay line: dung, khong tu xoay di tim.
    if (action == LINE_STOP) {
        tracking.previousAction = LINE_STOP;
        tracking.crossing = false;
        stopMotion();
        return;
    }

    // Bo qua mot lan doc bat thuong. Hai lan giong nhau thi moi chay.
    if (action != tracking.previousAction) {
        tracking.previousAction = action;
        return;
    }

    const int speed = motorBaseSpeed;
    const int power = MOTOR_OUTPUT_PERCENT; // Chi cac lenh auto o day dung power nay.

    // Ca 8 mat den: di thang qua nga tu, toi da 140 ms roi dung neu van con den.
    if (action == LINE_CROSS) {
        if (!tracking.crossing) {
            tracking.crossing = true;
            tracking.crossStartedAt = now;
        }
        if (now - tracking.crossStartedAt < CROSS_LINE_PASS_DELAY_MS) {
            runLR(speed, speed, power);
        } else {
            stopMotion();
        }
        return;
    }
    tracking.crossing = false;

    // Ba lenh co ban: thang, re trai, re phai.
    if (action == LINE_FORWARD) {
        runLR(speed, speed, power);
    } else if (action == LINE_LEFT) {
        runLR(0, speed, power); // Dung banh trai, chay banh phai.
    } else if (action == LINE_RIGHT) {
        runLR(speed, 0, power); // Chay banh trai, dung banh phai.
    }
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
