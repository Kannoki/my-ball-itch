#ifndef LINE_SENSOR_H
#define LINE_SENSOR_H

#include "config.h"

/**
 * @brief Muc logic dang duoc dung de phat hien vach den (HIGH hoac LOW)
 */
extern uint8_t lineActiveState;
extern bool sensorPullupEnabled;
extern uint8_t edgeFollowMode;

/**
 * @brief Dao nguoc muc logic giua HIGH va LOW trong khi dang chay de test
 */
void toggleLineActiveState();

/**
 * @brief Chuyen doi che do bam vien (Trang Trai/Den Phai <-> Den Trai/Trang Phai <-> Auto)
 */
void toggleEdgeFollowMode();
const char* getEdgeFollowModeName();

/**
 * @brief Dao bat/tat tro keo INPUT_PULLUP vs INPUT tren 8 chan sensor
 */
void toggleSensorPullup();
void setSensorPullup(bool enable);

/**
 * @brief Bat hoac tat 8 den LED phat hong ngoai (qua chan PIN_LINE_IR_EN / GPIO 1)
 */
void setSensorIrEmitter(bool enable);
bool isSensorIrEmitterEnabled();

/**
 * @brief Doc truc tiep gia tri dien ap 0/1 tren 8 chan GPIO cua ESP32
 */
uint8_t readDirectGpioPins();

/**
 * @brief Khoi tao 8 chan GPIO cho cam bien do line
 */
void lineSensorInit();

/**
 * @brief Doc gia tri tho cua 8 mat cam bien
 * @return Byte 8 bit, bit 0 tuong ung S1 (trai nhat), bit 7 tuong ung S8 (phai nhat)
 *         Bit = 1 la dang phat hien vach line, 0 la khong phat hien
 */
uint8_t readRawSensors();

/**
 * @brief Tra ve chuoi trang thai gon gang gui qua BLE UART (R:00110000 [..##....] E:+0.0)
 */
String getSensorTelemetryCompact();

/**
 * @brief Tra ve bao cao chi tiet tung chan, muc dien ap va giai ma logic cho lenh '?'
 */
String getSensorTelemetryDetailed();

/**
 * @brief Tinh toan vi tri lech tam cua duong line
 * @return Gia tri sai so vi tri tu -3.5 (lech han sang trai) den +3.5 (lech han sang phai)
 *         0.0 la line dang o giua (S4 & S5)
 */
float getLinePosition();

struct LineEdge {
    float position;
    EdgeMode polarity;
};

// Tim mep gan vi tri tham chieu nhat tu mot mau cam bien. Vi tri tu -3 den +3.
bool findLineEdge(uint8_t raw, uint8_t mode, float reference, LineEdge& edge);

/**
 * @brief Kiem tra xe co dang nam hoan toan tren nen Trang khong
 */
bool isAllWhite();

/**
 * @brief Kiem tra xe co dang nam hoan toan tren vung Den khong
 */
bool isAllBlack();

/**
 * @brief Kiem tra xem xe co bi mat line hay khong (khong co mat nao thay line)
 */
bool isLineLost();

/**
 * @brief Kiem tra xem co phai vach ngang / nga tu (tat ca hoac phan lon mat deu kich hoat)
 */
bool isCrossLine();

/**
 * @brief Luu lai vi tri lech cuoi cung truoc khi mat line
 */
float getLastKnownPosition();

/**
 * @brief In trang thai truc quan 8 mat cam bien len Serial Monitor de can chinh
 */
void printSensorDebug();

#endif // LINE_SENSOR_H
