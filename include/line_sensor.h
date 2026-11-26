#ifndef LINE_SENSOR_H
#define LINE_SENSOR_H

#include "config.h"

extern uint8_t lineActiveState;
extern bool sensorPullupEnabled;

// Moi mau cam bien chi dan den mot trong nam hanh dong nay.
enum LineAction {
    LINE_STOP,
    LINE_FORWARD,
    LINE_LEFT,
    LINE_RIGHT,
    LINE_CROSS
};

void lineSensorInit();
void toggleLineActiveState();
void toggleSensorPullup();
void setSensorPullup(bool enable);
void setSensorIrEmitter(bool enable);
bool isSensorIrEmitterEnabled();

// Van doc du 8 mat. Bit 0 la S1; bit 1 la S2; ... bit 7 la S8.
uint8_t readDirectGpioPins();
uint8_t readRawSensors(); // Bit 1 = black theo LINE_ACTIVE_STATE.

// Cung mot bo luat cho dieu khien va hien thi BLE/Serial.
LineAction getLineAction(uint8_t raw);
const char* getLineActionName(LineAction action);
String getSensorTelemetryCompact();
String getSensorTelemetryDetailed();
void printSensorDebug();

#endif // LINE_SENSOR_H
