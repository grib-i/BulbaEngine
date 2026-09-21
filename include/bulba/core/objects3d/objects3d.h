#ifndef OBJECTS3D_H
#define OBJECTS3D_H

#include "bulba/core/entity.h"
#include "bulba/core/math3v/math3v.h"
#include "bulba/core/math3v/polygon.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/render_mode.h"
#include "bulba/core/utils/object.h"

#include <stdbool.h>

typedef struct BLB_RigidBody BLB_RigidBody;
typedef struct BLB_Collider BLB_Collider;

typedef struct {
  BLB_EntityId entity_id;
  BLB_ComponentMask component_mask;

  BLB_Material *material;
  BLB_Texture *texture;

  BLB_ObjectType type;
  BLB_ObjectID *id;

  BLB_RigidBody *rigid_body;
  BLB_Collider *collider;

  HMM_Vec3 position;
  HMM_Vec3 rotation;
  HMM_Vec3 scale;

  BLB_Polygon3D *polygon;

  Mesh mesh;

  BLB_RenderMode render_mode;

  float *delta_time;

  unsigned char color[4];

  float glow;
  float emission;
  float roundness;

  unsigned short layer;

  bool visible;
} BLB_Object3D;

void BLB_Object3D_Move(BLB_Object3D *object, HMM_Vec3 velocity);

void BLB_Object3D_SetPosition(BLB_Object3D *object, HMM_Vec3 position);

void BLB_Object3D_Rotate(BLB_Object3D *object, HMM_Vec3 angular_velocity);

void BLB_Object3D_SetRotation(BLB_Object3D *object, HMM_Vec3 rotation);

void BLB_Object3D_Scale(BLB_Object3D *object, HMM_Vec3 scale_velocity);

void BLB_Object3D_SetScale(BLB_Object3D *object, HMM_Vec3 scale);

void BLB_Object3D_Transform(BLB_Object3D *object, HMM_Vec3 position, HMM_Vec3 rotation, HMM_Vec3 scale);

void BLB_Object3D_SetTexture(BLB_Object3D *object, BLB_Texture *texture);

#endif
