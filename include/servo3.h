#ifndef SERVO3_H
#define SERVO3_H

#include "config.h"

/**
 * @file servo3.h
 * @brief Module dieu khien doc lap cho Servo 3 (Chan GPIO 47)
 */

void servo3_init();
void servo3_setAngle(int angle);
int  servo3_getAngle();
void servo3_update(); // Goi trong loop() de chay animation/state machine sau nay

#endif // SERVO3_H
