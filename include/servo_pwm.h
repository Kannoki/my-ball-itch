#ifndef SERVO_PWM_H
#define SERVO_PWM_H

#include <Arduino.h>

/**
 * @brief Khoi tao kenh LEDC doc lap 50Hz (14-bit) cho mot servo va dat xung ban dau.
 */
void servoPwmInit(int pin, int channel, int initialUs);

/**
 * @brief Xuat xung PWM (microseconds) ra kenh LEDC cua servo.
 */
void servoPwmWriteUs(int pin, int channel, int us);

#endif // SERVO_PWM_H
