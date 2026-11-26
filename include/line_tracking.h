#ifndef LINE_TRACKING_H
#define LINE_TRACKING_H

#include <stdint.h>

void updateLineTracking(bool enabled);
uint32_t getCrossReverseDuration();
void setCrossReverseDuration(uint32_t durationMs);

#endif
