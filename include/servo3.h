#ifndef SERVO3_H
#define SERVO3_H

#include "config.h"

/**
 * @file servo3.h
 * @brief Module dieu khien doc lap cho Servo 3 (Chan GPIO 47)
 */

void servo3_init();
void servo3_setMicroseconds(int us);
void servo3_setAngle(int angle);
void servo3_setPercent(int percent);
int  servo3_getAngle();
int  servo3_getMicroseconds();
void servo3_update();

#endif // SERVO3_H
