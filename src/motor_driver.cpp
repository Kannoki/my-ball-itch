#include "motor_driver.h"
#include <esp_arduino_version.h>
#include <mutex>

static_assert(MOTOR_MIN_PWM_LEFT > 0 && MOTOR_MIN_PWM_RIGHT > 0 &&
              MOTOR_MIN_SPEED <= MOTOR_MAX_SPEED && MOTOR_BASE_SPEED <= MOTOR_MAX_SPEED && MOTOR_MAX_SPEED <= 255,
              "Invalid motor PWM limits");
static_assert(MOTOR_START_PWM >= MOTOR_MIN_SPEED && MOTOR_START_PWM <= MOTOR_MAX_SPEED && MOTOR_START_MS > 0,
              "Invalid motor startup boost");
int motorBaseSpeed = max(MOTOR_BASE_SPEED, MOTOR_MIN_SPEED);

struct MotorChannel {
    const int in1;
    const int in2;
    const int pwmPin;
    const int pwmChannel;
    int direction;
    int requestedDuty;
    unsigned long boostStartedAt;
    bool boosting;
};

static MotorChannel channelB{
    PIN_MOTOR_LEFT_IN3, PIN_MOTOR_LEFT_IN4, PIN_MOTOR_LEFT_ENB, PWM_CH_LEFT, 0, 0, 0, false
};
static MotorChannel channelA{
    PIN_MOTOR_RIGHT_IN1, PIN_MOTOR_RIGHT_IN2, PIN_MOTOR_RIGHT_ENA, PWM_CH_RIGHT, 0, 0, 0, false
};
static MotorChannel& leftMotor = SWAP_LEFT_RIGHT_MOTORS ? channelA : channelB;
static MotorChannel& rightMotor = SWAP_LEFT_RIGHT_MOTORS ? channelB : channelA;

// Lenh BLE chay tren task khac voi vong lap cap nhat PWM.
static std::mutex motorMutex;

void motorDriverInit() {
    // Cau hinh cac chan huong IN1..IN4 la OUTPUT
    pinMode(PIN_MOTOR_RIGHT_IN1, OUTPUT);
    pinMode(PIN_MOTOR_RIGHT_IN2, OUTPUT);
    pinMode(PIN_MOTOR_LEFT_IN3, OUTPUT);
    pinMode(PIN_MOTOR_LEFT_IN4, OUTPUT);

    // Dừng động cơ ban đầu
    digitalWrite(PIN_MOTOR_RIGHT_IN1, LOW);
    digitalWrite(PIN_MOTOR_RIGHT_IN2, LOW);
    digitalWrite(PIN_MOTOR_LEFT_IN3, LOW);
    digitalWrite(PIN_MOTOR_LEFT_IN4, LOW);

    // Thiết lập PWM cho ENB (OUT3/4) và ENA (OUT1/2).
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    // Arduino ESP32 Core 3.x
    ledcAttach(PIN_MOTOR_LEFT_ENB, PWM_FREQ, PWM_RESOLUTION);
    ledcAttach(PIN_MOTOR_RIGHT_ENA, PWM_FREQ, PWM_RESOLUTION);
    ledcWrite(PIN_MOTOR_LEFT_ENB, 0);
    ledcWrite(PIN_MOTOR_RIGHT_ENA, 0);
#else
    // Arduino ESP32 Core 2.x
    ledcSetup(PWM_CH_LEFT, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_MOTOR_LEFT_ENB, PWM_CH_LEFT);
    ledcWrite(PWM_CH_LEFT, 0);

    ledcSetup(PWM_CH_RIGHT, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_MOTOR_RIGHT_ENA, PWM_CH_RIGHT);
    ledcWrite(PWM_CH_RIGHT, 0);
#endif

    Serial.println(SWAP_LEFT_RIGHT_MOTORS ? "[Motor] L298N Trai=OUT1/2, Phai=OUT3/4"
                                         : "[Motor] L298N Trai=OUT3/4, Phai=OUT1/2");
}

static void writeMotorPwm(const MotorChannel& motor, int duty) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(motor.pwmPin, duty);
#else
    ledcWrite(motor.pwmChannel, duty);
#endif
}

static void endBoostIfReady(MotorChannel& motor, unsigned long now) {
    if (motor.boosting && now - motor.boostStartedAt >= MOTOR_START_MS) {
        motor.boosting = false;
        writeMotorPwm(motor, motor.requestedDuty);
    }
}

void motorDriverUpdate() {
    static unsigned long lastUpdate = 0;
    const unsigned long tick = millis();
    if (tick == lastUpdate) return;
    lastUpdate = tick;

    const std::lock_guard<std::mutex> lock(motorMutex);
    const unsigned long now = millis();
    endBoostIfReady(leftMotor, now);
    endBoostIfReady(rightMotor, now);
}

static void setMotorSpeed(MotorChannel& motor, int speed, int minimumDuty, unsigned long now) {
    const int direction = (speed > 0) - (speed < 0);
    if (direction != motor.direction) {
        writeMotorPwm(motor, 0);
        digitalWrite(motor.in1, direction > 0 ? HIGH : LOW);
        digitalWrite(motor.in2, direction < 0 ? HIGH : LOW);

        motor.boosting = (motor.direction == 0 && direction != 0);
        motor.boostStartedAt = now;
        motor.direction = direction;
    }

    motor.requestedDuty = direction == 0 ? 0 : max(abs(speed), minimumDuty);
    if (motor.boosting && now - motor.boostStartedAt >= MOTOR_START_MS) motor.boosting = false;
    const int duty = motor.boosting ? max(motor.requestedDuty, MOTOR_START_PWM) : motor.requestedDuty;
    writeMotorPwm(motor, duty);
}

void setMotorSpeeds(int leftSpeed, int rightSpeed) {
    leftSpeed = constrain(leftSpeed, -255, 255);
    rightSpeed = constrain(rightSpeed, -255, 255);
#if INVERT_LEFT_MOTORS
    leftSpeed = -leftSpeed;
#endif
#if INVERT_RIGHT_MOTORS
    rightSpeed = -rightSpeed;
#endif

    const std::lock_guard<std::mutex> lock(motorMutex);
    const unsigned long now = millis();
    setMotorSpeed(leftMotor, leftSpeed, MOTOR_MIN_PWM_LEFT, now);
    setMotorSpeed(rightMotor, rightSpeed, MOTOR_MIN_PWM_RIGHT, now);
}

void moveForward(int speed) {
    setMotorSpeeds(speed, speed);
}

void moveBackward(int speed) {
    setMotorSpeeds(-speed, -speed);
}

void turnLeft(int speed) {
    setMotorSpeeds(speed / 3, speed);
}

void turnRight(int speed) {
    setMotorSpeeds(speed, speed / 3);
}

void spinLeft(int speed) {
    setMotorSpeeds(-speed, speed);
}

void spinRight(int speed) {
    setMotorSpeeds(speed, -speed);
}

void stopMotors() {
    setMotorSpeeds(0, 0);
}
