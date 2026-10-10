#include "bulba/core/objects3d/model_loader_glb.h"
#include "bulba/core/objects3d/model_loader_assimp.h"

BLB_Object3D *BLB_LoadModelGLB(const char *path, HMM_Vec3 scale, HMM_Vec3 position, BLB_Texture *texture, BLB_TextureProjection mapping) {
  return BLB_LoadModelImported(path, scale, position, texture, mapping);
}

void BLB_DestroyModelGLB(BLB_Object3D *object) { BLB_DestroyModelImported(object); }
