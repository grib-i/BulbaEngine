#include "bulba/core/utils/fps.h"
#include "bulba/core/platform.h"
#include "bulba/graphics/vulkan/renderer.h"

#include <time.h>

int BLB_InitFPS(BLB_FPS *f, VULKAN *v, int limit, bool vsync) {
  if (!f || !v)
    return -1;

  f->last_time = 0.0;
  f->value = 0.0f;
  f->delta_time = 0.0f;

  if (limit >= 0)
    f->limit = limit;

  if (VULKAN_RendererRecreateSwapchain(v, vsync) != 0)
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

      if (sleep_time > 0.0) {
        struct timespec ts;

        ts.tv_sec = (time_t)sleep_time;
        ts.tv_nsec = (long)((sleep_time - (double)ts.tv_sec) * 1000000000.0);

        if (ts.tv_nsec < 0)
          ts.tv_nsec = 0;

        if (ts.tv_nsec >= 1000000000L) {
          ts.tv_sec++;
          ts.tv_nsec -= 1000000000L;
        }

        nanosleep(&ts, NULL);
      }
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

int BLB_FPS_SetVSync(VULKAN *v, bool vsync) {
  if (!v)
    return -1;

  return VULKAN_RendererRecreateSwapchain(v, vsync);
}

void BLB_FPS_SetLimit(BLB_FPS *f, int limit) {
  if (!f)
    return;

  f->limit = limit;
}

float BLB_GetFPS(const BLB_FPS *f) { return f ? f->value : 0.0f; }

float BLB_GetDeltaTime(const BLB_FPS *f) { return f ? f->delta_time : 0.0f; }
