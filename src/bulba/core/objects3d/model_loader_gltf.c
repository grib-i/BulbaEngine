#include "bulba/core/objects3d/model_loader_gltf.h"
#include "bulba/core/objects3d/model_loader_assimp.h"

BLB_Object3D *BLB_LoadModelGLTF(const char *path, BLB_Vec3 scale, BLB_Vec3 position, BLB_Texture *texture, BLB_TextureProjection mapping) {
  return BLB_LoadModelImported(path, scale, position, texture, mapping);
}

void BLB_DestroyModelGLTF(BLB_Object3D *object) { BLB_DestroyModelImported(object); }
