#include "servo3.h"
#include <ESP32Servo.h>

static Servo myServo3;
static int currentAngle3 = SERVO3_MAX_ANGLE / 2; // Vi tri ban dau 90 do
static int currentMicroseconds3 = 1500;           // Neutral 1500us

void servo3_init() {
    ESP32PWM::allocateTimer(3);
    myServo3.setPeriodHertz(50); // 50 Hz chuan frame 20ms
    int ch = myServo3.attach(PIN_SERVO_3, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US);
    servo3_setMicroseconds(1500);
    Serial.printf("[Servo 3] Khoi tao thanh cong GPIO %d (Channel %d) | Xung: %d-%dus | Goc Max: %d do | Neutral: %dus\n",
                  PIN_SERVO_3, ch, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US, SERVO3_MAX_ANGLE, currentMicroseconds3);
}

void servo3_setMicroseconds(int us) {
    currentMicroseconds3 = constrain(us, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US);
    myServo3.writeMicroseconds(currentMicroseconds3);
    currentAngle3 = map(currentMicroseconds3, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US, 0, SERVO3_MAX_ANGLE);
    Serial.printf("[Servo 3] Dat xung truc tiep: %dus (~%d do)\n", currentMicroseconds3, currentAngle3);
}

void servo3_setAngle(int angle) {
    currentAngle3 = constrain(angle, 0, SERVO3_MAX_ANGLE);
    currentMicroseconds3 = map(currentAngle3, 0, SERVO3_MAX_ANGLE, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US);
    myServo3.writeMicroseconds(currentMicroseconds3);
    Serial.printf("[Servo 3] Dat goc: %d/%d do (Xung: %dus)\n", currentAngle3, SERVO3_MAX_ANGLE, currentMicroseconds3);
}

void servo3_setPercent(int percent) {
    percent = constrain(percent, 0, 100);
    int us = map(percent, 0, 100, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US);
    servo3_setMicroseconds(us);
    Serial.printf("[Servo 3] Dat hanh trinh: %d%% (%dus)\n", percent, us);
}

int servo3_getAngle() {
    return currentAngle3;
}

int servo3_getMicroseconds() {
    return currentMicroseconds3;
}

void servo3_update() {
    // Day la noi dat logic/task tuy bien cho Servo 3 ma ban se them sau
}
