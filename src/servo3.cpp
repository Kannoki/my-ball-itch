#include "servo3.h"
#include "servo_pwm.h"

static int currentAngle3 = SERVO3_MAX_ANGLE / 2; // Vi tri ban dau 90 do
static int currentMicroseconds3 = 1500;           // Neutral 1500us

void servo3_init() {
    currentMicroseconds3 = constrain(1500, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US);
    currentAngle3 = map(currentMicroseconds3, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US, 0, SERVO3_MAX_ANGLE);
    servoPwmInit(PIN_SERVO_3, PWM_CH_SERVO3, currentMicroseconds3);
    Serial.printf("[Servo 3] Khoi tao thanh cong GPIO %d (LEDC ch %d) | Xung: %d-%dus | Goc Max: %d do | Neutral: %dus\n",
                  PIN_SERVO_3, PWM_CH_SERVO3, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US, SERVO3_MAX_ANGLE, currentMicroseconds3);
}

void servo3_setMicroseconds(int us) {
    currentMicroseconds3 = constrain(us, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US);
    servoPwmWriteUs(PIN_SERVO_3, PWM_CH_SERVO3, currentMicroseconds3);
    currentAngle3 = map(currentMicroseconds3, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US, 0, SERVO3_MAX_ANGLE);
    Serial.printf("[Servo 3] Dat xung truc tiep: %dus (~%d do)\n", currentMicroseconds3, currentAngle3);
}

void servo3_setAngle(int angle) {
    currentAngle3 = constrain(angle, 0, SERVO3_MAX_ANGLE);
    currentMicroseconds3 = map(currentAngle3, 0, SERVO3_MAX_ANGLE, SERVO3_MIN_PULSE_US, SERVO3_MAX_PULSE_US);
    servoPwmWriteUs(PIN_SERVO_3, PWM_CH_SERVO3, currentMicroseconds3);
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
