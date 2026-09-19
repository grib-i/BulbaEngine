#ifndef SQUARE_H
#define SQUARE_H

#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/render/texture.h"

BLB_Object2D *BLB_CreateSquare2D(HMM_Vec2 scale, HMM_Vec2 position, BLB_Texture *texture, bool screen_space);
void BLB_DestroySquare2D(BLB_Object2D *object);

#endif
