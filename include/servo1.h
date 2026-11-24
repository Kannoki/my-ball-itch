#ifndef SERVO1_H
#define SERVO1_H

#include "config.h"

/**
 * @file servo1.h
 * @brief Module dieu khien doc lap cho Servo TD-8120MG tren Chan GPIO 14
 *        Ho tro dai xung 500us - 2500us, goc 0 - 270 do full hanh trinh
 */

void servo1_init();
void servo1_setAngle(int angle);           // Dat goc tu 0 den 270 do
void servo1_setMicroseconds(int us);       // Dat xung truc tiep tu 500us den 2500us
void servo1_setPercent(int percent);       // Dat 0% den 100% hanh trinh day
int  servo1_getAngle();
int  servo1_getMicroseconds();
void servo1_update();                      // Goi trong loop()

#endif // SERVO1_H
