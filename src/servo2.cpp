#include "servo2.h"
#include <ESP32Servo.h>

static Servo myServo2;
static int currentAngle2 = SERVO2_MAX_ANGLE / 2; // Vi tri ban dau o giua
static int currentMicroseconds2 = 1500;           // Xung neutral 1500us
static unsigned long lastUpdate2 = 0;

void servo2_init() {
    ESP32PWM::allocateTimer(3);
    myServo2.setPeriodHertz(50); // 50 Hz chuan frame 20ms
    // Cau hinh dai xung full theo datasheet TD-8120MG: 500us den 2500us
    myServo2.attach(PIN_SERVO_2, SERVO2_MIN_PULSE_US, SERVO2_MAX_PULSE_US);
    
    // Dua ve vi tri trung tam (Neutral 1500us)
    servo2_setMicroseconds(1500);

    Serial.printf("[Servo 2 - TD8120MG] Khoi tao GPIO %d | Xung: %d-%dus | Goc Max: %d do | Vi tri ban dau: %dus\n",
                  PIN_SERVO_2, SERVO2_MIN_PULSE_US, SERVO2_MAX_PULSE_US, SERVO2_MAX_ANGLE, currentMicroseconds2);
}

void servo2_setMicroseconds(int us) {
    currentMicroseconds2 = constrain(us, SERVO2_MIN_PULSE_US, SERVO2_MAX_PULSE_US);
    myServo2.writeMicroseconds(currentMicroseconds2);
    currentAngle2 = map(currentMicroseconds2, SERVO2_MIN_PULSE_US, SERVO2_MAX_PULSE_US, 0, SERVO2_MAX_ANGLE);
    Serial.printf("[Servo 2 - TD8120MG] Dat xung truc tiep: %dus (~%d do)\n", currentMicroseconds2, currentAngle2);
}

void servo2_setAngle(int angle) {
    currentAngle2 = constrain(angle, 0, SERVO2_MAX_ANGLE);
    // Anh xa tuyen tinh goc tu 0..SERVO2_MAX_ANGLE sang 500us..2500us de mo tron 100% hanh trinh
    currentMicroseconds2 = map(currentAngle2, 0, SERVO2_MAX_ANGLE, SERVO2_MIN_PULSE_US, SERVO2_MAX_PULSE_US);
    myServo2.writeMicroseconds(currentMicroseconds2);
    Serial.printf("[Servo 2 - TD8120MG] Dat goc: %d/%d do (Xung: %dus)\n", currentAngle2, SERVO2_MAX_ANGLE, currentMicroseconds2);
}

void servo2_setPercent(int percent) {
    percent = constrain(percent, 0, 100);
    int us = map(percent, 0, 100, SERVO2_MIN_PULSE_US, SERVO2_MAX_PULSE_US);
    servo2_setMicroseconds(us);
    Serial.printf("[Servo 2 - TD8120MG] Dat hanh trinh: %d%% (%dus)\n", percent, us);
}

int servo2_getAngle() {
    return currentAngle2;
}

int servo2_getMicroseconds() {
    return currentMicroseconds2;
}

void servo2_update() {
    // Day la noi dat logic/task tuy bien cho Servo 2 ma ban se them sau
    // Vi du: day hanh trinh, kep nha, nang ha canh tay...
}
