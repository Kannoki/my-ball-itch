#ifndef LINE_TRACKING_H
#define LINE_TRACKING_H

#include "config.h"

// Chi danh cho hai cau hinh IR 8 mat. Khong ghi GPIO trong bo dieu khien nay.
void resetLineTracking();
void updateLineTracking(uint8_t raw, unsigned long now, int baseSpeedPwm);
int getLineLeftPwm();
int getLineRightPwm();
const char* getLineTrackingStatus();

#endif
