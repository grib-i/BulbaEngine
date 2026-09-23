#ifndef OBJECTS2D_H
#define OBJECTS2D_H

#include "bulba/core/entity.h"
#include "bulba/core/math3v/math3v.h"
#include "bulba/core/math3v/polygon.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/render_mode.h"
#include "bulba/core/utils/object.h"

#include <stdbool.h>

typedef struct {
  BLB_Texture **textures;
  short texture_counter;
  size_t textures_count;

  float frame_time;
  bool enable;
  float frame_count;
} BLB_Animation;

typedef struct {
  BLB_EntityId entity_id;
  BLB_ComponentMask component_mask;

  BLB_Material *material;
  BLB_Texture *texture;
  BLB_Animation *animation;

  BLB_ObjectType type;
  BLB_ObjectID *id;

  HMM_Vec2 position;
  float rotation;
  HMM_Vec2 scale;

  BLB_Polygon2D *polygon;

  BLB_RenderMode render_mode;

  float *delta_time;

  unsigned char color[4];

  float glow;
  float emission;
  float roundness;

  unsigned short layer;

  bool visible;
  bool screen_space;
} BLB_Object2D;

void BLB_Object2D_Move(BLB_Object2D *object, HMM_Vec2 velocity);

void BLB_Object2D_SetPosition(BLB_Object2D *object, HMM_Vec2 position);

void BLB_Object2D_Rotate(BLB_Object2D *object, float angular_velocity);

void BLB_Object2D_SetRotation(BLB_Object2D *object, float rotation);

void BLB_Object2D_Scale(BLB_Object2D *object, HMM_Vec2 scale_velocity);

void BLB_Object2D_SetScale(BLB_Object2D *object, HMM_Vec2 scale);

void BLB_Object2D_Transform(BLB_Object2D *object, HMM_Vec2 position, float rotation, HMM_Vec2 scale);

void BLB_Object2D_SetTexture(BLB_Object2D *object, BLB_Texture *texture);
void BLB_Object2D_SetMaterial(BLB_Object2D *object, BLB_Material *material);

void BLB_SetAnimation(BLB_Object2D *object, BLB_Texture **textures, float frame_time, size_t textures_count);

void BLB_StartAnimation(BLB_Object2D *object);

void BLB_StopAnimation(BLB_Object2D *object);

void BLB_Object2D_FlipX(BLB_Object2D *object);
void BLB_Object2D_FlipY(BLB_Object2D *object);
void BLB_Object2D_FlipX(BLB_Object2D *object);

#endif
