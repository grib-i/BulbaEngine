#ifndef BULBA_CORE_UTILS_FPS_H
#define BULBA_CORE_UTILS_FPS_H

#include "bulba/graphics/renderer.h"
#include <stdbool.h>

void BLB_UpdateFPS(void);

void BLB_FPS_SetLimit(int limit);
int BLB_FPS_SetVSync(BLB_Renderer *renderer, bool vsync);

float BLB_FPS_GetValue(void);
int BLB_FPS_GetLimit(void);

#endif
