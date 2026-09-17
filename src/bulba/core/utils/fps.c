#include "bulba/core/utils/fps.h"

void BLB_InitFPS(BLB_FPS *f) {
  if (!f)
    return;

  f->last_time = 0.0;
  f->value = 0.0f;
  f->delta_time = 0.0f;
}

void BLB_UpdateFPS(BLB_FPS *f, double now) {
  if (!f)
    return;

  if (f->last_time > 0.0 && now > f->last_time) {
    double dt = now - f->last_time;

    if (dt > 0.0) {
      f->delta_time = (float)dt;
      f->value = (float)(1.0 / dt);
    }
  }

  f->last_time = now;
}

float BLB_GetFPS(const BLB_FPS *f) { return f ? f->value : 0.0f; }

float BLB_GetDeltaTime(const BLB_FPS *f) { return f ? f->delta_time : 0.0f; }
