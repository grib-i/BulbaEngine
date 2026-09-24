#ifndef BLB_MODEL_LOADER_H
#define BLB_MODEL_LOADER_H

#include "bulba/core/objects3d/objects3d.h"

BLB_Object3D *BLB_LoadModelOBJ(const char *path, HMM_Vec3 scale, HMM_Vec3 position, BLB_Texture *texture);

void BLB_DestroyModelOBJ(BLB_Object3D *object);

#endif
