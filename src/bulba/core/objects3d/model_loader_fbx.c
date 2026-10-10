#include "bulba/core/objects3d/model_loader_fbx.h"
#include "bulba/core/objects3d/model_loader_assimp.h"

BLB_Object3D *BLB_LoadModelFBX(const char *path, HMM_Vec3 scale, HMM_Vec3 position, BLB_Texture *texture, BLB_TextureProjection mapping) {
  return BLB_LoadModelImported(path, scale, position, texture, mapping);
}

void BLB_DestroyModelFBX(BLB_Object3D *object) { BLB_DestroyModelImported(object); }
