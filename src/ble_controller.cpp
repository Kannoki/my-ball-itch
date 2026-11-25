#include "ble_controller.h"
#include "motor_driver.h"
#include "line_sensor.h"
#include "servo1.h"
#include "servo2.h"
#include "servo3.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <mutex>

// UUID chuan Nordic UART Service dung boi Adafruit Bluefruit LE Connect app
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

RobotMode currentRobotMode = MODE_STANDBY_BLE; // Mac dinh khi khoi dong la DUNG YEN CHO
bool bleTelemetryStreamEnabled = true;          // Mac dinh truyen luong trang thai sensor ve BLE

void toggleBleTelemetryStream() {
    bleTelemetryStreamEnabled = !bleTelemetryStreamEnabled;
}

void setBleTelemetryStream(bool enable) {
    bleTelemetryStreamEnabled = enable;
}

static BLEServer* pServer = nullptr;
static BLECharacteristic* pTxCharacteristic = nullptr;
static bool deviceConnected = false;

// Ham xu ly goi tin Controller (!B...) tu app Adafruit Bluefruit
static void handleControllerPacket(const uint8_t* data, size_t len) {
    if (len < 4) return;
    if (data[0] != '!' || data[1] != 'B') return;

    char button = (char)data[2];
    bool pressed = (data[3] == '1');

    Serial.printf("[BLE] Controller Button '%c': %s\n", button, pressed ? "PRESSED" : "RELEASED");

    // 1. Phim 1: KHOI DONG / TAM DUNG DO VIEN LINE
    if (button == '1' && pressed) {
        if (currentRobotMode == MODE_LINE_TRACKING) {
            currentRobotMode = MODE_STANDBY_BLE;
            stopMotors();
            bleSendLog("Trang thai: TAM DUNG (Standby). Bam '1' de tiep tuc do vien.\n");
            Serial.println("[Mode] TAM DUNG DO VIEN (Standby)");
        } else {
            currentRobotMode = MODE_LINE_TRACKING;
            bleSendLog("Trang thai: BAT DAU DO VIEN (Edge Tracking)!\n");
            Serial.println("[Mode] BAT DAU DO VIEN (Edge Tracking)");
        }
        return;
    }

    // 2. Phim 2: KICH HOAT SERVO 1 (TD-8120MG) day kịch 100% hanh trinh
    if (button == '2' && pressed) {
        // Toggle giua 0 do (500us - thu ve) va 270 do (2500us - day het co 100%)
        int target = (servo1_getAngle() < SERVO1_MAX_ANGLE / 2) ? SERVO1_MAX_ANGLE : 0;
        servo1_setAngle(target);
        bleSendLog("Servo 1 (TD-8120MG / Pin " + String(PIN_SERVO_1) + "): " + String(target) + " deg (" + String(servo1_getMicroseconds()) + "us)\n");
        return;
    }
    // Phim 3: KICH HOAT SERVO 2
    if (button == '3' && pressed) {
        int target = (servo2_getAngle() <= 90) ? 180 : 0;
        servo2_setAngle(target);
        bleSendLog("Servo 2 (Pin " + String(PIN_SERVO_2) + "): " + String(target) + " deg\n");
        return;
    }
    // Phim 4: KICH HOAT SERVO 3
    if (button == '4' && pressed) {
        int target = (servo3_getAngle() <= 90) ? 180 : 0;
        servo3_setAngle(target);
        bleSendLog("Servo 3 (Pin " + String(PIN_SERVO_3) + "): " + String(target) + " deg\n");
        return;
    }

    // 3. Phim mui ten D-Pad: 5=UP, 6=DOWN, 7=LEFT, 8=RIGHT
    // Khi o che do Standby, cho phep test dieu khien tay
    if (currentRobotMode == MODE_STANDBY_BLE) {
        if (pressed) {
            switch (button) {
                case '5': // UP - Tien
                    moveForward(MOTOR_MANUAL_SPEED);
                    break;
                case '6': // DOWN - Lui
                    moveBackward(MOTOR_MANUAL_SPEED);
                    break;
                case '7': // LEFT - Re trai
                    spinLeft(MOTOR_MANUAL_SPEED);
                    break;
                case '8': // RIGHT - Re phai
                    spinRight(MOTOR_MANUAL_SPEED);
                    break;
                default:
                    break;
            }
        } else {
            // Khi nha nut dieu huong -> dung xe
            if (button >= '5' && button <= '8') {
                stopMotors();
            }
        }
    }
}

// Ham xu ly ky tu / chuoi text tu man hinh UART Terminal cua Bluefruit app
static void handleUartText(const String& text) {
    String cmd = text;
    cmd.trim();
    if (cmd.length() == 0) return;

    Serial.printf("[BLE UART] Nhan: %s\n", cmd.c_str());

    char c = cmd.charAt(0);

    // Tro giup / Bao cao chan doan sensor chi tiet: ?, H hoac h
    if (c == '?' || c == 'H' || c == 'h') {
        bleSendLog(getSensorTelemetryDetailed());
        return;
    }

    // Bat / tat den LED phat hong ngoai (IR Emitter): E hoac e
    if (c == 'E' || c == 'e') {
#if defined(PIN_LINE_IR_EN) && (PIN_LINE_IR_EN >= 0)
        bool newState = !isSensorIrEmitterEnabled();
        setSensorIrEmitter(newState);
        bleSendLog("IR EMITTER (GPIO " + String(PIN_LINE_IR_EN) + ") -> " + String(newState ? "BAT (HIGH)\n" : "TAT (LOW)\n"));
#else
        bleSendLog("IR EMITTER: KHONG DUNG tren module nay\n");
#endif
        return;
    }

    // Dao tro keo INPUT_PULLUP vs INPUT tren 8 chan sensor: P hoac p
    if (c == 'P' || c == 'p') {
        toggleSensorPullup();
        bleSendLog("SENSOR PULLUP -> " + String(sensorPullupEnabled ? "BAT (INPUT_PULLUP)\n" : "TAT (INPUT)\n"));
        return;
    }

    // Bat / tat luong truyen telemetry truc tiep: T hoac t
    if (c == 'T' || c == 't') {
        toggleBleTelemetryStream();
        bleSendLog("TELEMETRY STREAM -> " + String(bleTelemetryStreamEnabled ? "BAT (ON)\n" : "TAT (OFF)\n"));
        return;
    }

    // Chuyen doi che do bam vien: V hoac v
    if (c == 'V' || c == 'v') {
        toggleEdgeFollowMode();
        bleSendLog("EDGE MODE -> " + String(getEdgeFollowModeName()) + "\n");
        return;
    }

    // Tang / Giam toc do co ban khi do line: '+' hoac '-'
    if (c == '+') {
        motorBaseSpeed = min(motorBaseSpeed + 10, MOTOR_MAX_SPEED);
        bleSendLog("MOTOR_BASE_SPEED -> " + String(motorBaseSpeed) + "\n");
        return;
    }
    if (c == '-') {
        motorBaseSpeed = max(motorBaseSpeed - 10, MOTOR_MIN_SPEED);
        bleSendLog("MOTOR_BASE_SPEED -> " + String(motorBaseSpeed) + "\n");
        return;
    }

    // Chuyen che do / Bat dau do vien: 1, M hoac m (khi chi co 1 ky tu hoac khong co tham so)
    if ((c == '1' || c == 'M' || c == 'm') && cmd.length() == 1) {
        if (currentRobotMode == MODE_LINE_TRACKING) {
            currentRobotMode = MODE_STANDBY_BLE;
            stopMotors();
            bleSendLog("Mode -> STANDBY (Tam dung)\n");
        } else {
            currentRobotMode = MODE_LINE_TRACKING;
            bleSendLog("Mode -> BAT DAU DO VIEN (Edge Tracking)\n");
        }
        return;
    }

    // Kich hoat Servo 1 kịch hanh trinh khi bam '2':
    if (c == '2' && cmd.length() == 1) {
        int target = (servo1_getAngle() < SERVO1_MAX_ANGLE / 2) ? SERVO1_MAX_ANGLE : 0;
        servo1_setAngle(target);
        bleSendLog("Servo 1 (Pin " + String(PIN_SERVO_1) + "): " + String(target) + " deg (" + String(servo1_getMicroseconds()) + "us)\n");
        return;
    }
    // Kich hoat Servo 2 khi bam '3' (tuong ung Nut 3):
    if (c == '3' && cmd.length() == 1) {
        int target = (servo2_getAngle() <= 90) ? 180 : 0;
        servo2_setAngle(target);
        bleSendLog("Servo 2 (Pin " + String(PIN_SERVO_2) + "): " + String(target) + " deg\n");
        return;
    }
    // Kich hoat Servo 3 khi bam '4' (tuong ung Nut 4):
    if (c == '4' && cmd.length() == 1) {
        int target = (servo3_getAngle() <= 90) ? 180 : 0;
        servo3_setAngle(target);
        bleSendLog("Servo 3 (Pin " + String(PIN_SERVO_3) + "): " + String(target) + " deg\n");
        return;
    }

    // Lenh dao muc logic cam bien: I hoac i
    if (c == 'I' || c == 'i') {
        toggleLineActiveState();
        bleSendLog("LINE_ACTIVE_STATE -> " + String((lineActiveState == HIGH) ? "HIGH (Vach den = 1)\n" : "LOW (Vach den = 0)\n"));
        return;
    }

    // Lenh di chuyen bang tay khi o Standby
    if (currentRobotMode == MODE_STANDBY_BLE) {
        switch (c) {
            case 'F': case 'f':
                moveForward(MOTOR_MANUAL_SPEED);
                bleSendLog("Moving Forward\n");
                break;
            case 'B': case 'b':
                moveBackward(MOTOR_MANUAL_SPEED);
                bleSendLog("Moving Backward\n");
                break;
            case 'L': case 'l':
                spinLeft(MOTOR_MANUAL_SPEED);
                bleSendLog("Turning Left\n");
                break;
            case 'R': case 'r':
                spinRight(MOTOR_MANUAL_SPEED);
                bleSendLog("Turning Right\n");
                break;
            case 'S': case 's':
                stopMotors();
                bleSendLog("Stopped\n");
                break;
            default:
                break;
        }
    }

    // Lenh dat goc servo truc tiep:
    // "2 <goc>" hoac "S1 <goc>" -> Servo 1 (Pin 14 - Nut 2)
    // "3 <goc>" hoac "S2 <goc>" -> Servo 2 (Pin 21 - Nut 3)
    // "4 <goc>" hoac "S3 <goc>" -> Servo 3 (Pin 47 - Nut 4)
    String upperCmd = cmd;
    upperCmd.toUpperCase();
    if (upperCmd.startsWith("S1 ") || (c == '2' && cmd.length() > 2) || (c == '1' && cmd.length() > 2)) {
        int spaceIdx = cmd.indexOf(' ');
        int val = cmd.substring(spaceIdx + 1).toInt();
        if (val > 360) {
            servo1_setMicroseconds(val);
            bleSendLog("Servo 1 (Pin " + String(PIN_SERVO_1) + ") Pulse: " + String(val) + "us\n");
        } else {
            servo1_setAngle(val);
            bleSendLog("Servo 1 (Pin " + String(PIN_SERVO_1) + ") Angle: " + String(val) + " deg (" + String(servo1_getMicroseconds()) + "us)\n");
        }
        return;
    }
    if (upperCmd.startsWith("S2 ") || (c == '3' && cmd.length() > 2)) {
        int spaceIdx = cmd.indexOf(' ');
        int val = cmd.substring(spaceIdx + 1).toInt();
        servo2_setAngle(val);
        bleSendLog("Servo 2 (Pin " + String(PIN_SERVO_2) + ") Angle: " + String(val) + " deg\n");
        return;
    }
    if (upperCmd.startsWith("S3 ") || (c == '4' && cmd.length() > 2)) {
        int spaceIdx = cmd.indexOf(' ');
        int val = cmd.substring(spaceIdx + 1).toInt();
        servo3_setAngle(val);
        bleSendLog("Servo 3 (Pin " + String(PIN_SERVO_3) + ") Angle: " + String(val) + " deg\n");
        return;
    }
}

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        deviceConnected = true;
        Serial.println("[BLE] Da ket noi voi App!");
    }

    void onDisconnect(BLEServer* pServer) override {
        deviceConnected = false;
        Serial.println("[BLE] Ngat ket noi! Tu dong quang ba lai ngay lap tuc...");
        // Luon luon bat BLE quang ba lai ngay khi ngat ket noi
        BLEDevice::startAdvertising();
    }
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            const uint8_t* rawData = (const uint8_t*)rxValue.data();
            size_t len = rxValue.length();

            // Neu bat dau bang '!B' -> Goi tin Controller Pad cua Bluefruit
            if (len >= 4 && rawData[0] == '!' && rawData[1] == 'B') {
                handleControllerPacket(rawData, len);
            } else {
                // Xu ly chuoi text UART thong thuong
                String str = String(rxValue.c_str());
                handleUartText(str);
            }
        }
    }
};

void bleControllerInit() {
    BLEDevice::init(BLE_DEVICE_NAME);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Khoi tao Nordic UART Service
    BLEService* pService = pServer->createService(SERVICE_UUID);

    // TX Characteristic: ESP32 truyen du lieu ve dien thoai (Notify)
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // RX Characteristic: Dien thoai ghi lenh xuong ESP32 (Write)
    BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new CharacteristicCallbacks());

    pService->start();

    // Cau hinh khong can mat khau (Open / Just Works / No PIN)
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_NO_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);

    // Bat dau phat quang ba (Advertising) lien tuc
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // Thiet lap giup ket noi on dinh tren iOS
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.printf("[BLE] BLE luon bat (khong mat khau), ten: '%s'\n", BLE_DEVICE_NAME);
}

static std::mutex bleTxMutex;
static String bleTxQueue;
static unsigned long lastBleNotifyAt = 0;
constexpr size_t BLE_TX_MAX_QUEUE = 512;
constexpr unsigned long BLE_NOTIFY_INTERVAL_MS = 12;

static void pumpBleTxQueue(unsigned long now) {
    if (!deviceConnected || pTxCharacteristic == nullptr) {
        const std::lock_guard<std::mutex> lock(bleTxMutex);
        bleTxQueue = "";
        return;
    }

    uint8_t chunk[20];
    size_t chunkSize = 0;
    {
        const std::lock_guard<std::mutex> lock(bleTxMutex);
        if (bleTxQueue.length() == 0 || (now - lastBleNotifyAt < BLE_NOTIFY_INTERVAL_MS)) {
            return;
        }
        chunkSize = min(static_cast<size_t>(20), static_cast<size_t>(bleTxQueue.length()));
        memcpy(chunk, bleTxQueue.c_str(), chunkSize);
        bleTxQueue.remove(0, chunkSize);
        lastBleNotifyAt = now;
    }

    pTxCharacteristic->setValue(chunk, chunkSize);
    pTxCharacteristic->notify();
}

void bleControllerUpdate() {
    static bool wasConnected = false;
    static bool retryAdvertising = false;
    static unsigned long disconnectedAt = 0;
    const bool connected = deviceConnected;
    const unsigned long now = millis();

    if (connected) {
        retryAdvertising = false;
    } else if (wasConnected) {
        disconnectedAt = now;
        retryAdvertising = true;
    }
    wasConnected = connected;

    if (retryAdvertising && now - disconnectedAt >= 500) {
        retryAdvertising = false;
        pServer->startAdvertising();
        Serial.println("[BLE] Bat dau quang ba lai...");
    }

    pumpBleTxQueue(now);
}

bool isBleConnected() {
    return deviceConnected;
}

void bleSendLog(const String& message) {
    if (!deviceConnected || pTxCharacteristic == nullptr || message.length() == 0) return;

    const std::lock_guard<std::mutex> lock(bleTxMutex);
    if (bleTxQueue.length() + message.length() <= BLE_TX_MAX_QUEUE) {
        bleTxQueue += message;
    }
}
