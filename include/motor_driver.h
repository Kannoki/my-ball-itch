#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include "config.h"

// Toc do co ban hien tai (co the tinh chinh qua BLE UART bang '+' hoac '-')
extern int motorBaseSpeed;

/**
 * @brief Khoi tao cac chan va kenh PWM dieu khien dong co qua L298N
 */
void motorDriverInit();

// Tra PWM ve muc yeu cau khi nhip de-pa ket thuc, ke ca khi khong co lenh BLE moi.
void motorDriverUpdate();

/**
 * @brief Dat toc do va huong cho ca 2 ben dong co
 * @param leftSpeed  Toc do ben trai (-255 den +255, so am la chay lui)
 * @param rightSpeed Toc do ben phai (-255 den +255, so am la chay lui)
 */
void setMotorSpeeds(int leftSpeed, int rightSpeed);

/**
 * @brief Di thang toi truoc
 * @param speed Toc do tu 0 den 255
 */
void moveForward(int speed);

/**
 * @brief Di lui
 * @param speed Toc do tu 0 den 255
 */
void moveBackward(int speed);

/**
 * @brief Re trai (dong co phai chay, dong co trai dung hoac chay cham)
 * @param speed Toc do
 */
void turnLeft(int speed);

/**
 * @brief Re phai (dong co trai chay, dong co phai dung hoac chay cham)
 * @param speed Toc do
 */
void turnRight(int speed);

/**
 * @brief Xoay tai cho sang trai (banh trai lui, banh phai toi)
 * @param speed Toc do xoay
 */
void spinLeft(int speed);

/**
 * @brief Xoay tai cho sang phai (banh trai toi, banh phai lui)
 * @param speed Toc do xoay
 */
void spinRight(int speed);

/**
 * @brief Dung toan bo 4 dong co
 */
void stopMotors();

#endif // MOTOR_DRIVER_H
