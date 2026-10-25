#ifndef BULBA_CORE_OBJECTS3D_MODEL_LOADER_ASSIMP_H
#define BULBA_CORE_OBJECTS3D_MODEL_LOADER_ASSIMP_H

#include "bulba/core/objects3d/objects3d.h"

BLB_Object3D *BLB_LoadModelImported(const char *path, BLB_Vec3 scale, BLB_Vec3 position, BLB_Texture *texture, BLB_TextureProjection mapping);
void BLB_DestroyModelImported(BLB_Object3D *object);

#endif
