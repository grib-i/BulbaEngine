#include "bulba/core/utils/delta_time.h"
#include "bulba/core/platform.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

static bool time_initialized = false;

static float delta_time = 0.0f;
static double last_time = 0.0f;

static uint64_t revision = 0;

float BLB_DeltaTime_Get(void) { return delta_time; }

uint64_t BLB_DeltaTime_GetRevision(void) { return revision; }

void BLB_DeltaTime_Set(float value) {
  if (!isfinite(value) || value < 0.0f)
    return;

  delta_time = value;

  ++revision;

  if (revision == 0)
    ++revision;
}

void BLB_DeltaTime_Update(void) {
  double now = BLB_Platform_TimeSeconds();

  if (!isfinite(now)) {
    BLB_DeltaTime_Set(0.0f);

    return;
  }

  if (!time_initialized) {
    last_time = now;
    time_initialized = true;
    BLB_DeltaTime_Set(0.0f);

    return;
  }

  if (now < last_time) {
    last_time = now;
    BLB_DeltaTime_Set(0.0f);

    return;
  }

  double elapsed = now - last_time;

  last_time = now;

  float dt = (float)elapsed;

  if (!isfinite(dt) || dt < 0.0f)
    dt = 0.0f;

  BLB_DeltaTime_Set(dt);
}
