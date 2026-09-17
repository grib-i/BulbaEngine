#ifndef BULBA_CORE_UTILS_FPS_H
#define BULBA_CORE_UTILS_FPS_H

typedef struct BLB_FPS {
  double last_time;
  float value;
  float delta_time;
} BLB_FPS;

void BLB_InitFPS(BLB_FPS *fps);
void BLB_UpdateFPS(BLB_FPS *fps, double time_now);

float BLB_GetFPS(const BLB_FPS *fps);
float BLB_GetDeltaTime(const BLB_FPS *fps);

#endif
