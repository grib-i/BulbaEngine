#ifndef OBJECTS3D_H
#define OBJECTS3D_H

#include "bulba/core/entity.h"
#include "bulba/core/math3v/math3v.h"
#include "bulba/core/math3v/polygon.h"
#include "bulba/core/physics/physics3d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/render_mode.h"
#include "bulba/core/utils/colors.h"
#include "bulba/core/utils/object.h"
#include "bulba/handlers/object.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct BLB_RenderObjects3DCluster BLB_RenderObjects3DCluster;
typedef struct BLB_RenderInstances3DGroup BLB_RenderInstances3DGroup;

typedef struct BLB_ObjectSubmesh {
  BLB_Polygon3D *polygon;
  BLB_Material *material;
} BLB_ObjectSubmesh;

typedef struct BLB_Object3D {
  BLB_EntityId entity_id;
  BLB_ComponentMask component_mask;

  BLB_Material *material;
  BLB_Texture *texture;

  BLB_ObjectType type;
  BLB_ObjectID *id;
  uint64_t geometry_id;

  BLB_RigidBody3D *rigid_body;
  BLB_Collider3D *collider;

  BLB_PhysicsSyncMode physics_sync_mode[BLB_TRANSFORM_TYPE_COUNT];
  BLB_Vec3 position;
  BLB_Vec3 rotation;
  BLB_Vec3 scale;
  uint64_t transform_revision;
  float bounds_radius;

  BLB_Polygon3D *polygon;

  Mesh mesh;

  BLB_Polygon3D **lod_polygons;
  size_t lod_polygon_count;
  int lod_level;

  BLB_RenderMode render_mode;

  BLB_RenderInstances3DGroup *optimization_instance_group;
  BLB_RenderObjects3DCluster *optimization_cluster;
  BLB_Vec3 optimization_last_position;
  BLB_Vec3 optimization_last_rotation;
  BLB_Vec3 optimization_last_scale;
  uint32_t optimization_stable_frames;
  bool optimization_dynamic;
  bool optimization_motion_initialized;
  bool optimization_transform_dirty;
  bool optimization_static_recovered;

  bool lod_override;
  bool lod_shared_cache;

  float delta_time;

  BLB_Color color;

  float roundness;

  unsigned short layer;

  bool visible;
  bool camera_attached;

  BLB_ObjectHandler *handler;

  BLB_ObjectSubmesh *submeshes;
  size_t submesh_count;
  void *model_asset;
  void *animation3d;
  bool owns_model_geometry;
} BLB_Object3D;

void BLB_Object3D_Move(BLB_Object3D *object, BLB_Vec3 velocity);

void BLB_Object3D_SetPosition(BLB_Object3D *object, BLB_Vec3 position);

void BLB_Object3D_Rotate(BLB_Object3D *object, BLB_Vec3 angular_velocity);

void BLB_Object3D_SetRotation(BLB_Object3D *object, BLB_Vec3 rotation);

void BLB_Object3D_Scale(BLB_Object3D *object, BLB_Vec3 scale_velocity);

void BLB_Object3D_SetScale(BLB_Object3D *object, BLB_Vec3 scale);

void BLB_Object3D_Transform(BLB_Object3D *object, BLB_Vec3 position,
                            BLB_Vec3 rotation, BLB_Vec3 scale);

void BLB_Object3D_SetCameraAttached(BLB_Object3D *object, bool attached);
bool BLB_Object3D_IsCameraAttached(const BLB_Object3D *object);

int BLB_Object3D_SetTexture(BLB_Object3D *object, BLB_Texture *texture);
void BLB_Object3D_SetMaterial(BLB_Object3D *object, BLB_Material *material);

int BLB_Object3D_SetMaterialFromFile(BLB_Object3D *object, const char *path);

void BLB_Object3D_FlipX(BLB_Object3D *object);
void BLB_Object3D_FlipY(BLB_Object3D *object);
void BLB_Object3D_FlipZ(BLB_Object3D *object);
void BLB_Object3D_SetHandler(BLB_Object3D *object,
                             const BLB_ObjectHandler *handler);
void BLB_Object3D_ClearHandler(BLB_Object3D *object);

void BLB_Object3D_SetColor(BLB_Object3D *object, BLB_Color color);

void BLB_Object3D_SetRoundness(BLB_Object3D *object, float roundness);

void BLB_Object3D_SetLayer(BLB_Object3D *object, unsigned short layer);

void BLB_Object3D_SetVisible(BLB_Object3D *object, bool visible);

void BLB_Object3D_SetLOD_Override(BLB_Object3D *object, bool override);

void BLB_Object3D_SetLOD_SharedCache(BLB_Object3D *object, bool shared_cache);

void BLB_Object3D_SetPhysicsSyncMode(BLB_Object3D *object,
                                     BLB_PhysicsSyncMode mode,
                                     BLB_TransformType in_transform);

#endif
