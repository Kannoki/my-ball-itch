#include "servo3.h"
#include <ESP32Servo.h>

static Servo myServo3;
static int currentAngle3 = 90; // Vi tri ban dau 90 do
static unsigned long lastUpdate3 = 0;

void servo3_init() {
    // Dung timer 0 hoac 1 con lai
    ESP32PWM::allocateTimer(1);
    myServo3.setPeriodHertz(50);
    myServo3.attach(PIN_SERVO_3, 500, 2400);
    myServo3.write(currentAngle3);
    Serial.printf("[Servo 3] Khoi tao thanh cong tai GPIO %d, goc ban dau: %d do\n", PIN_SERVO_3, currentAngle3);
}

void servo3_setAngle(int angle) {
    currentAngle3 = constrain(angle, 0, 180);
    myServo3.write(currentAngle3);
    Serial.printf("[Servo 3] Dat goc: %d do\n", currentAngle3);
}

int servo3_getAngle() {
    return currentAngle3;
}

void servo3_update() {
    // Day la noi dat logic/task tuy bien cho Servo 3 ma ban se them sau
    // Vi du: xoay goc cam bien sieu am hoac camera quan sat...
    //
    // unsigned long now = millis();
    // if (now - lastUpdate3 >= 20) {
    //     lastUpdate3 = now;
    //     // TODO: Them logic cua ban o day
    // }
}
