#ifndef BULBA_CORE_UTILS_FPS_H
#define BULBA_CORE_UTILS_FPS_H

#include "bulba/graphics/vulkan/swapchain.h"
#include "bulba/graphics/vulkan/vulkan.h"
#include <stdbool.h>

typedef struct BLB_FPS {
  double last_time;
  float value;
  float delta_time;
  float limit;
} BLB_FPS;

int BLB_InitFPS(BLB_FPS *f, VULKAN *v, int limit, bool vsync);
void BLB_UpdateFPS(BLB_FPS *fps);

void BLB_FPS_SetLimit(BLB_FPS *f, int limit);
int BLB_FPS_SetVSync(VULKAN *v, bool vsync);

float BLB_GetFPS(const BLB_FPS *fps);
float BLB_GetDeltaTime(const BLB_FPS *fps);

#endif
