#ifndef TORUS_H
#define TORUS_H

#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/texture.h"

BLB_Object3D *BLB_CreateTorus3D(HMM_Vec3 scale, HMM_Vec3 position, int level_of_detail, float outer_radius, float hole_radius, BLB_Texture *texture);
void BLB_DestroyTorus3D(BLB_Object3D *object);

#endif
