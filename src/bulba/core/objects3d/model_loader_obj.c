#include "bulba/core/objects3d/model_loader_obj.h"
#include "bulba/core/objects3d/model_loader_assimp.h"

/*
 * Use the same importer as FBX/GLTF for robust OBJ handling. Assimp handles
 * negative and independently indexed position/UV/normal indices, triangulates
 * polygons and imports mtllib/usemtl material ranges (including map_Kd).
 */
BLB_Object3D *BLB_LoadModelOBJ(const char *path, HMM_Vec3 scale, HMM_Vec3 position, BLB_Texture *texture, BLB_TextureProjection mapping) {
  return BLB_LoadModelImported(path, scale, position, texture, mapping);
}

void BLB_DestroyModelOBJ(BLB_Object3D *object) { BLB_DestroyModelImported(object); }
