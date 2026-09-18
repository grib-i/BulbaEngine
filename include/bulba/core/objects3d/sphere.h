#ifndef SPHERE_H
#define SPHERE_H

#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/texture.h"

BLB_Object3D *BLB_CreateSphere3D(HMM_Vec3 scale, HMM_Vec3 position, int level_of_detail, BLB_Texture *texture);
void BLB_DestroySphere3D(BLB_Object3D *object);

#endif
