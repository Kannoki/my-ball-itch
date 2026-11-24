#ifndef BLE_CONTROLLER_H
#define BLE_CONTROLLER_H

#include "config.h"

// Bien trang thai che do hoat dong hien tai
extern RobotMode currentRobotMode;

// Bat / tat luong truyen telemetry trang thai sensor qua BLE UART
extern bool bleTelemetryStreamEnabled;

/**
 * @brief Dao bat / tat luong du lieu telemetry gui qua BLE
 */
void toggleBleTelemetryStream();
void setBleTelemetryStream(bool enable);

/**
 * @brief Khoi tao Bluetooth Low Energy tuong thich app Adafruit Bluefruit LE Connect
 */
void bleControllerInit();

void bleControllerUpdate();

/**
 * @brief Kiem tra xem co thiet bi nao dang ket noi BLE khong
 */
bool isBleConnected();

/**
 * @brief Gui chuoi thong bao phan hoi ve ung dung Bluefruit LE
 */
void bleSendLog(const String& message);

#endif // BLE_CONTROLLER_H
