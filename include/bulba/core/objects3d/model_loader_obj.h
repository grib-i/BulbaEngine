#ifndef BLB_MODEL_LOADER_OBJ_H
#define BLB_MODEL_LOADER_OBJ_H

#include "bulba/core/objects3d/objects3d.h"

BLB_Object3D *BLB_LoadModelOBJ(const char *path, BLB_Vec3 scale,
                               BLB_Vec3 position, BLB_Texture *texture,
                               BLB_TextureProjection mapping);

void BLB_DestroyModelOBJ(BLB_Object3D *object);

#endif
