#ifndef BLB_MODEL_LOADER_GLB_H
#define BLB_MODEL_LOADER_GLB_H

#include "bulba/core/objects3d/objects3d.h"

BLB_Object3D *BLB_LoadModelGLB(const char *path, BLB_Vec3 scale,
                               BLB_Vec3 position, BLB_Texture *texture,
                               BLB_TextureProjection mapping);

void BLB_DestroyModelGLB(BLB_Object3D *object);

#endif
