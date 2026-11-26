#include <Arduino.h>
#include "config.h"
#include "motor_driver.h"
#include "line_sensor.h"
#include "line_tracking.h"
#include "servo1.h"
#include "servo2.h"
#include "servo3.h"
#include "ble_controller.h"

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

void loop() {
    motorDriverUpdate();
    bleControllerUpdate();

    // 2. Cap nhat cac task Servo chay rieng biet
    servo1_update();
    servo2_update();
    servo3_update();

    // 3. Xu ly di chuyen tuy theo Che do
    updateLineTracking(currentRobotMode == MODE_LINE_TRACKING);

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
