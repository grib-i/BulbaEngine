#ifndef OBJECTS2D_H
#define OBJECTS2D_H

#include "bulba/core/entity.h"
#include "bulba/core/math3v/math3v.h"
#include "bulba/core/math3v/polygon.h"
#include "bulba/core/physics/physics2d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/render_mode.h"
#include "bulba/core/utils/object.h"
#include "bulba/handlers/object.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  BLB_Texture **textures;
  size_t texture_counter;
  size_t textures_count;

  float frame_time;
  bool enable;
  float frame_count;
} BLB_Animation;

typedef struct BLB_Object2D {
  BLB_EntityId entity_id;
  BLB_ComponentMask component_mask;

  BLB_Material *material;
  BLB_Texture *texture;
  BLB_Animation *animation;

  BLB_ObjectType type;
  BLB_ObjectID *id;

  BLB_RigidBody2D *rigid_body;
  BLB_Collider2D *collider;
  BLB_PhysicsSyncMode physics_sync_mode[BLB_TRANSFORM_TYPE_COUNT];

  BLB_Vec2 position;
  float rotation;
  BLB_Vec2 scale;

  BLB_Polygon2D *polygon;

  BLB_Polygon2D **lod_polygons;
  size_t lod_polygon_count;
  int lod_level;
  bool lod_override;
  bool lod_shared_cache;

  BLB_RenderMode render_mode;

  float delta_time;

  BLB_Vec4 color;

  float roundness;

  unsigned short layer;

  bool visible;
  bool camera_attached;

  BLB_ObjectHandler *handler;
  void *optimization_instance_group;
} BLB_Object2D;

void BLB_Object2D_Move(BLB_Object2D *object, BLB_Vec2 velocity);

void BLB_Object2D_SetPosition(BLB_Object2D *object, BLB_Vec2 position);

void BLB_Object2D_Rotate(BLB_Object2D *object, float angular_velocity);

void BLB_Object2D_SetRotation(BLB_Object2D *object, float rotation);

void BLB_Object2D_Scale(BLB_Object2D *object, BLB_Vec2 scale_velocity);

void BLB_Object2D_SetScale(BLB_Object2D *object, BLB_Vec2 scale);

void BLB_Object2D_Transform(BLB_Object2D *object, BLB_Vec2 position, float rotation, BLB_Vec2 scale);

int BLB_Object2D_SetTexture(BLB_Object2D *object, BLB_Texture *texture);
void BLB_Object2D_SetMaterial(BLB_Object2D *object, BLB_Material *material);

void BLB_Object2D_SetCameraAttached(BLB_Object2D *object, bool attached);
bool BLB_Object2D_IsCameraAttached(const BLB_Object2D *object);

int BLB_Object2D_SetMaterialFromFile(BLB_Object2D *object, const char *path);

void BLB_SetAnimation(BLB_Object2D *object, BLB_Texture **textures, float frame_time, size_t textures_count);

void BLB_ClearAnimation(BLB_Object2D *object);
BLB_Texture *BLB_Object2D_GetTexture(const BLB_Object2D *object);

void BLB_Object2D_UpdateAnimation(BLB_Object2D *object, float delta_time);

void BLB_StartAnimation(BLB_Object2D *object);

void BLB_StopAnimation(BLB_Object2D *object);

void BLB_Object2D_FlipX(BLB_Object2D *object);
void BLB_Object2D_FlipY(BLB_Object2D *object);
void BLB_Object2D_SetHandler(BLB_Object2D *object, const BLB_ObjectHandler *handler);
void BLB_Object2D_ClearHandler(BLB_Object2D *object);

void BLB_Object2D_SetColor(BLB_Object2D *object, BLB_Vec4 color);

void BLB_Object2D_SetPhysicsSyncMode(BLB_Object2D *object, BLB_PhysicsSyncMode mode, BLB_TransformType transform);
#endif
