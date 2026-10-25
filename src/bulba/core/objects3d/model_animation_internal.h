#ifndef BLB_MODEL_ANIMATION_INTERNAL_H
#define BLB_MODEL_ANIMATION_INTERNAL_H

#include "bulba/core/objects3d/objects3d.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct aiNode;

typedef struct {
  double time;
  BLB_Vec3 value;
} BLB_ModelVectorKey;

typedef struct {
  double time;
  BLB_Quat value;
} BLB_ModelQuaternionKey;

typedef struct {
  char *name;
  int parent_index;
  BLB_Vec3 bind_position;
  BLB_Quat bind_rotation;
  BLB_Vec3 bind_scale;
  BLB_Mat4 bind_local;
  BLB_Mat4 bind_residual;
  BLB_Mat4 bind_global;
  /* valid only while the assimp scene is alive. clean before import returns. */
  const struct aiNode *source_node;
  bool is_bone;
} BLB_ModelAnimationNode;

typedef struct {
  int node_index;
  BLB_ModelVectorKey *positions;
  size_t position_count;
  BLB_ModelQuaternionKey *rotations;
  size_t rotation_count;
  BLB_ModelVectorKey *scales;
  size_t scale_count;
} BLB_ModelAnimationChannel;

typedef struct {
  char *name;
  double duration_ticks;
  double ticks_per_second;
  BLB_ModelAnimationChannel *channels;
  size_t channel_count;
} BLB_ModelAnimationClip;

typedef struct {
  uint32_t node_indices[4];
  float weights[4];
} BLB_ModelSkinVertex;

typedef struct {
  BLB_ModelSkinVertex *vertices;
  BLB_Mat4 *offsets;
  size_t vertex_count;
} BLB_ModelSkinSubmesh;

typedef struct {
  BLB_ModelAnimationNode *nodes;
  size_t node_count;
  BLB_ModelAnimationClip *clips;
  size_t clip_count;
  BLB_ModelSkinSubmesh *skins;
  size_t skin_count;
  size_t skin_capacity;
  BLB_Mat4 root_inverse;
} BLB_ModelAnimationAsset;

typedef struct {
  BLB_ModelAnimationAsset *asset;
  BLB_Vec3 *positions;
  BLB_Quat *rotations;
  BLB_Vec3 *scales;
  BLB_Vec3 *manual_positions;
  BLB_Quat *manual_rotations;
  BLB_Vec3 *manual_scales;
  uint8_t *manual_overrides;
  uint8_t *transform_dirty;
  BLB_Mat4 *global_transforms;
  BLB_Polygon3D **submesh_polygons;

  BLB_Vec3 **source_normals;
  BLB_Vec3 **bind_pose_vertices;
  BLB_Vec3 **bind_pose_normals;
  size_t submesh_count;
  BLB_Polygon3D *combined_polygon;
  bool bind_pose_captured;
  int active_clip;
  double time_ticks;
  float speed;
  bool loop;
  bool playing;
} BLB_ModelAnimationInstance;

void BLB_ModelAnimationAsset_Destroy(BLB_ModelAnimationAsset *asset);
int BLB_ModelAnimationInstance_Create(BLB_Object3D *object, BLB_ModelAnimationAsset *asset);
void BLB_ModelAnimationInstance_Destroy(BLB_Object3D *object);

#endif
