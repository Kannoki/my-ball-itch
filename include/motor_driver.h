#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include "config.h"

// Toc do co ban hien tai (don vi chuan hoa -1000..+1000).
// Co the tinh chinh qua BLE UART bang '+' hoac '-'.
extern int motorBaseSpeed;

// ==============================================================================
// CAU HINH RUNTIME (co the doi qua BLE ma KHONG can nap lai firmware)
// ==============================================================================
struct MotorRuntimeConfig {
    bool invertLeft;      // Dao chieu quay ben trai (khi ben phai lap doi doi xung co hoc)
    bool invertRight;     // Dao chieu quay ben phai
    bool swapLeftRight;   // Hoan doi kenh vat ly cua 2 ben
    int minPwmLeft;       // Duty nho nhat ma ben trai van chay duoc (0-255)
    int minPwmRight;      // Duty nho nhat ma ben phai van chay duoc (0-255)
    int deadbandLeft;     // Yeu cau duty nho hon -> DUNG THAT (0-255)
    int deadbandRight;    // Yeu cau duty nho hon -> DUNG THAT (0-255)
};

const MotorRuntimeConfig& getMotorRuntimeConfig();

// Tra ve mot ban sao de sua doi, dung ham setter de ghi lai (dong bo bo nho).
MotorRuntimeConfig getMotorConfigCopy();
void applyMotorRuntimeConfig(const MotorRuntimeConfig& config);

// Tien ich doi nhanh
void toggleInvertLeft();
void toggleInvertRight();
void toggleSwapLeftRight();
void setMotorMinPwm(int left, int right);
void setMotorDeadband(int left, int right);

// ==============================================================================
// API DIEU KHIEN (Van toc do chuan hoa -1000..+1000, LAY CAM HUNG LbMotion.runLR)
// ==============================================================================
// Frame cua robot: (+) = tien, (-) = lui cho CA HAI ben.
// Moi lenh deu dung nghiep duyen ke ca khi doi INVERT_*/SWAP_*/ENABLE*.

/**
 * @brief Khoi tao cac chan va kenh PWM dieu khien dong co qua L298N
 */
void motorDriverInit();

/**
 * @brief Cap nhat trang thai driver (nhip de-pa, ramp) va chan cac lenh khac.
 * @return true neu vua can phai cham vao dong co
 */
bool motorDriverUpdate();

/**
 * @brief API chinh: dat van toc do cho 2 ben dong co.
 * @param vL Van toc do ben trai (-1000 den +1000, so am la chay lui)
 * @param vR Van toc do ben phai (-1000 den +1000, so am la chay lui)
 */
// Lenh thuong mac dinh 100%; auto truyen MOTOR_OUTPUT_PERCENT rieng.
// immediate=true cho auto: bo de-pa/ramp de khong tre lenh re/dung.
void runLR(int vL, int vR, int powerPercent = 100, bool immediate = false);

/**
 * @brief Di thang toi truoc (ca hai ben cung chieu)
 */
void runForward(int speed);

/**
 * @brief Di lui
 */
void runBackward(int speed);

/**
 * @brief Xoay tai cho sang TRAI (ben phai chay tien, ben trai chay lui)
 */
void runTurnLeft(int speed);

/**
 * @brief Xoay tai cho sang PHAI (ben trai chay tien, ben phai chay lui)
 */
void runTurnRight(int speed);

/**
 * @brief Re cong (ben trong chay cham, ben ngoai chay nhanh) - khong bao gio doi chieu
 */
void runArcLeft(int speed);
void runArcRight(int speed);

/**
 * @brief Dung toan bo dong co (duty = 0, tha chan huong)
 */
void stopMotion();

// ==============================================================================
// WRAPPER CU (giu de ma cu khong bi hong)
// ==============================================================================
// Van toc do cua cac ham duoi day dung thang don vi PWM 0-255 nhu truoc.
// Moi chuyen sang dung runLR / runTurn* cho nhung lenh moi.

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

// ==============================================================================
// CHAN DOAN & TU KIEM TRA
// ==============================================================================

/**
 * @brief Bao cao chan doan dong co (trang thai GPIO, duty, co cuc)
 */
String getMotorDiagnostics();

/**
 * @brief Tu kiem tra: quay lan luot ben trai tien, ben phai tien, ben trai lui, ben phai lui.
 * @param stepMs Thoi gian moi pha (ms)
 * @return true neu da chay xong
 */
bool runMotorSelfTest(unsigned long stepMs = 300);

#endif // MOTOR_DRIVER_H
