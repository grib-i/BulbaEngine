#ifndef CIRCLE_H
#define CIRCLE_H

#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/render/texture.h"

BLB_Object2D *BLB_CreateCircle2D(HMM_Vec2 scale, HMM_Vec2 position, int level_of_detail, BLB_Texture *texture, bool screen_space);
void BLB_DestroyCircle2D(BLB_Object2D *object);

#endif
