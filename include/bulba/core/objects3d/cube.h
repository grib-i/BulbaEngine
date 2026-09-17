#ifndef CUBE3D_H
#define CUBE3D_H

#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/texture.h"

BLB_Object3D *BLB_CreateCube3D(HMM_Vec3 scale, HMM_Vec3 position, BLB_Texture *texture);
void BLB_DestroyCube3D(BLB_Object3D *object);

#endif
