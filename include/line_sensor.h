#ifndef LINE_SENSOR_H
#define LINE_SENSOR_H

#include "config.h"

extern uint8_t lineActiveState;
#ifndef SENSOR_PHOTORESISTOR
extern bool sensorPullupEnabled;
#endif

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
#ifndef SENSOR_PHOTORESISTOR
void toggleSensorPullup();
void setSensorPullup(bool enable);
#endif
void setSensorIrEmitter(bool enable);
bool isSensorIrEmitterEnabled();

// Bit 0 la S1. Bit = 1 khi mat tuong ung thay vach den theo LINE_ACTIVE_STATE.
uint8_t readRawSensors();
#ifdef SENSOR_PHOTORESISTOR
// Tra ve cung mat na den va gia tri ADC S1..S8 cua cung mot lan doc.
uint8_t readPhotoresSensors(uint16_t (&adcValues)[NUM_SENSORS]);
#endif

// Cung mot bo luat cho dieu khien va hien thi BLE/Serial.
LineAction getLineAction(uint8_t raw);
const char* getLineActionName(LineAction action);
String getSensorTelemetryCompact();
String getSensorTelemetryDetailed();
void printSensorDebug();

#endif // LINE_SENSOR_H
