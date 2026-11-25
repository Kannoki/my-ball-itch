#include "servo1.h"
#include "servo_pwm.h"

static int currentAngle1 = SERVO1_MAX_ANGLE / 2; // Vi tri ban dau o giua
static int currentMicroseconds1 = 1500;           // Xung neutral 1500us

void servo1_init() {
    currentMicroseconds1 = constrain(1500, SERVO1_MIN_PULSE_US, SERVO1_MAX_PULSE_US);
    currentAngle1 = map(currentMicroseconds1, SERVO1_MIN_PULSE_US, SERVO1_MAX_PULSE_US, 0, SERVO1_MAX_ANGLE);
    servoPwmInit(PIN_SERVO_1, PWM_CH_SERVO1, currentMicroseconds1);

    Serial.printf("[Servo 1 - TD8120MG] Khoi tao GPIO %d (LEDC ch %d) | Xung: %d-%dus | Goc Max: %d do | Neutral: %dus\n",
                  PIN_SERVO_1, PWM_CH_SERVO1, SERVO1_MIN_PULSE_US, SERVO1_MAX_PULSE_US, SERVO1_MAX_ANGLE, currentMicroseconds1);
}

void servo1_setMicroseconds(int us) {
    currentMicroseconds1 = constrain(us, SERVO1_MIN_PULSE_US, SERVO1_MAX_PULSE_US);
    servoPwmWriteUs(PIN_SERVO_1, PWM_CH_SERVO1, currentMicroseconds1);
    currentAngle1 = map(currentMicroseconds1, SERVO1_MIN_PULSE_US, SERVO1_MAX_PULSE_US, 0, SERVO1_MAX_ANGLE);
    Serial.printf("[Servo 1 - TD8120MG] Dat xung: %dus (~%d do)\n", currentMicroseconds1, currentAngle1);
}

void servo1_setAngle(int angle) {
    currentAngle1 = constrain(angle, 0, SERVO1_MAX_ANGLE);
    // Anh xa tuyen tinh goc tu 0..SERVO1_MAX_ANGLE sang 500us..2500us de mo tron 100% hanh trinh
    currentMicroseconds1 = map(currentAngle1, 0, SERVO1_MAX_ANGLE, SERVO1_MIN_PULSE_US, SERVO1_MAX_PULSE_US);
    servoPwmWriteUs(PIN_SERVO_1, PWM_CH_SERVO1, currentMicroseconds1);
    Serial.printf("[Servo 1 - TD8120MG] Dat goc: %d/%d do (Xung: %dus)\n", currentAngle1, SERVO1_MAX_ANGLE, currentMicroseconds1);
}

void servo1_setPercent(int percent) {
    percent = constrain(percent, 0, 100);
    int us = map(percent, 0, 100, SERVO1_MIN_PULSE_US, SERVO1_MAX_PULSE_US);
    servo1_setMicroseconds(us);
    Serial.printf("[Servo 1 - TD8120MG] Dat hanh trinh: %d%% (%dus)\n", percent, us);
}

int servo1_getAngle() {
    return currentAngle1;
}

int servo1_getMicroseconds() {
    return currentMicroseconds1;
}

void servo1_update() {
    // Day la noi dat logic/task tuy bien cho Servo 1 ma ban se them sau
}
