#ifndef SERVO2_H
#define SERVO2_H

#include "config.h"

/**
 * @file servo2.h
 * @brief Module dieu khien doc lap cho Servo TD-8120MG (Chan GPIO 21)
 *        Ho tro dai xung 500us - 2500us, goc 0 - 270 do full hanh trinh
 */

void servo2_init();
void servo2_setAngle(int angle);           // Dat goc tu 0 den 270 do
void servo2_setMicroseconds(int us);       // Dat xung truc tiep tu 500us den 2500us
void servo2_setPercent(int percent);       // Dat 0% den 100% hanh trinh day
int  servo2_getAngle();
int  servo2_getMicroseconds();
void servo2_update();                      // Goi trong loop()

#endif // SERVO2_H
