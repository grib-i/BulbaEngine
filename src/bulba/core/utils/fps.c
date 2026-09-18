#include "bulba/core/utils/fps.h"
#include "bulba/core/platform.h"
#include "debug.h"

#include <math.h>
#include <time.h>
#include <unistd.h>

#define max(a, b) ((a) > (b) ? (a) : (b))

int BLB_InitFPS(BLB_FPS *f, VULKAN *v, int limit, bool vsync) {
  if (!f)
    return -1;

  f->last_time = 0.0;
  f->value = 0.0f;
  f->delta_time = 0.0f;

  if (limit)
    f->limit = limit;

  VULKAN_DestroySwapchain(v);
  if (VULKAN_CreateSwapchain(v, vsync) != 0)
    return -1;

  return 0;
}

void BLB_UpdateFPS(BLB_FPS *f) {
  if (!f)
    return;

  double now;
  now = BLB_Platform_TimeSeconds();
  if (f->last_time > 0.0 && now > f->last_time) {
    if (f->limit != 0) {
      double sleep_time = (1.0 / (double)f->limit) - (now - f->last_time);

      struct timespec ts;
      ts.tv_sec = (time_t)sleep_time;
      ts.tv_nsec = (long)((sleep_time - (double)ts.tv_sec) * 1000000000.0);

      nanosleep(&ts, NULL);
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
  VULKAN_DestroySwapchain(v);
  if (VULKAN_CreateSwapchain(v, vsync) != 0)
    return -1;

  return 0;
}

void BLB_FPS_SetLimit(BLB_FPS *f, int limit) { f->limit = limit; }

float BLB_GetFPS(const BLB_FPS *f) { return f ? f->value : 0.0f; }

float BLB_GetDeltaTime(const BLB_FPS *f) { return f ? f->delta_time : 0.0f; }
