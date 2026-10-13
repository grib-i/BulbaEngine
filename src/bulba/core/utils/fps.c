#include "bulba/core/utils/fps.h"
#include "bulba/core/platform.h"


int BLB_InitFPS(BLB_FPS *f, BLB_Renderer *renderer, int limit, bool vsync) {
  if (!f || !renderer)
    return -1;

  f->last_time = 0.0;
  f->value = 0.0f;
  f->delta_time = 0.0f;

  if (limit >= 0)
    f->limit = limit;

  if (BLB_RendererGetVSync(renderer) != vsync && BLB_RendererRecreateSwapchain(renderer, vsync) != 0)
    return -1;

  return 0;
}

void BLB_UpdateFPS(BLB_FPS *f) {
  if (!f)
    return;

  double now = BLB_Platform_TimeSeconds();

  if (f->last_time > 0.0 && now > f->last_time) {
    if (f->limit > 0) {
      double target = 1.0 / (double)f->limit;
      double elapsed = now - f->last_time;
      double sleep_time = target - elapsed;

      if (sleep_time > 0.0)
        BLB_Platform_SleepSeconds(sleep_time);
    }

    now = BLB_Platform_TimeSeconds();

    double dt = now - f->last_time;

    if (dt > 0.0) {
      f->delta_time = (float)dt;
      f->value = (float)(1.0 / dt);
    }
  }

  f->last_time = now;
}

int BLB_FPS_SetVSync(BLB_Renderer *renderer, bool vsync) {
  if (!renderer)
    return -1;

  return BLB_RendererRecreateSwapchain(renderer, vsync);
}

void BLB_FPS_SetLimit(BLB_FPS *f, int limit) {
  if (!f)
    return;

  f->limit = limit;
}

float BLB_GetFPS(const BLB_FPS *f) { return f ? f->value : 0.0f; }

float BLB_GetDeltaTime(const BLB_FPS *f) { return f ? f->delta_time : 0.0f; }
