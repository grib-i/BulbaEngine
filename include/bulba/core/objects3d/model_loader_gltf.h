#ifndef BLB_MODEL_LOADER_GLTF_H
#define BLB_MODEL_LOADER_GLTF_H

#include "bulba/core/objects3d/objects3d.h"

BLB_Object3D *BLB_LoadModelGLTF(const char *path, HMM_Vec3 scale, HMM_Vec3 position, BLB_Texture *texture, BLB_TextureProjection mapping);
void BLB_DestroyModelGLTF(BLB_Object3D *object);

#endif
