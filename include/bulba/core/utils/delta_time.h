#ifndef BULBA_CORE_UTILS_DELTA_TIME_H
#define BULBA_CORE_UTILS_DELTA_TIME_H

#include <stdint.h>

float BLB_DeltaTime_Get(void);
void BLB_DeltaTime_Set(float delta_time);
void BLB_DeltaTime_Update(void);
uint64_t BLB_DeltaTime_GetRevision(void);

#endif
