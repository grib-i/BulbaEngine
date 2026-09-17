#ifndef CUBE2D_H
#define CUBE2D_H

#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/render/texture.h"

BLB_Object2D *BLB_CreateCube2D(HMM_Vec2 scale, HMM_Vec2 position, BLB_Texture *texture);
void BLB_DestroyCube2D(BLB_Object2D *object);

#endif
