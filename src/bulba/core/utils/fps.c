#include "bulba/core/utils/fps.h"
#include "bulba/core/platform.h"
#include "bulba/core/utils/config.h"

#include <math.h>
#include <stdbool.h>

static double last_frame_time = 0.0f;
static bool time_initialized = false;

static float fps_value = 0.0f;
static int fps_limit = 0;

void BLB_UpdateFPS(void) {
  double now = BLB_Platform_TimeSeconds();

  if (!isfinite(now)) {
    fps_value = 0.0f;

    return;
  }

  if (!time_initialized) {
    last_frame_time = now;
    time_initialized = true;
    fps_value = 0.0f;

    return;
  }

  if (now < last_frame_time) {
    last_frame_time = now;
    fps_value = 0.0f;

    return;
  }

  if (fps_limit > 0) {
    double target = 1.0 / (double)fps_limit;
    double elapsed = now - last_frame_time;
    double sleep_time = target - elapsed;

    if (sleep_time > 0.0) {
      BLB_Platform_SleepSeconds(sleep_time);
      now = BLB_Platform_TimeSeconds();
    }
  }

  if (!isfinite(now)) {
    fps_value = 0.0f;

    return;
  }

  if (now < last_frame_time) {
    last_frame_time = now;
    fps_value = 0.0f;

    return;
  }

  double elapsed = now - last_frame_time;

  if (elapsed > 0.0 && isfinite(elapsed)) {
    float fps_instant = (float)(1.0 / elapsed);

    /* smoothing the FPS value using an exponential method */
    float alpha = 1.0f - expf(-(float)elapsed / BLB_FPS_SMOOTHING_TIME);

    if (fps_value <= 0.0f)
      fps_value = fps_instant;
    else
      fps_value += alpha * (fps_instant - fps_value);

    last_frame_time = now;
  } else {
    fps_value = 0.0f;
  }
}

int BLB_FPS_SetVSync(BLB_Renderer *renderer, bool vsync) {
  if (!renderer)
    return -1;

  return BLB_RendererSetVSync(renderer, vsync);
}

void BLB_FPS_SetLimit(int limit) {
  fps_limit = limit > 0 ? limit : 0;
}

float BLB_FPS_GetValue(void) {
  return fps_value;
}

int BLB_FPS_GetLimit(void) {
  return fps_limit;
}
