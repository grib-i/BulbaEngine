#ifndef OBJECTS2D_H
#define OBJECTS2D_H

#include "bulba/core/entity.h"
#include "bulba/core/math3v/math3v.h"
#include "bulba/core/math3v/polygon.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/render_mode.h"

#include <stdbool.h>

typedef struct {
  BLB_EntityId entity_id;
  BLB_ComponentMask component_mask;

  BLB_Material *material;
  BLB_Texture *texture;

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
} BLB_Object2D;

void BLB_Move2D(BLB_Object2D *object, HMM_Vec2 velocity);

void BLB_SetPosition2D(BLB_Object2D *object, HMM_Vec2 position);

void BLB_Rotate2D(BLB_Object2D *object, float angular_velocity);

void BLB_SetRotation2D(BLB_Object2D *object, float rotation);

void BLB_Scale2D(BLB_Object2D *object, HMM_Vec2 scale_velocity);

void BLB_SetScale2D(BLB_Object2D *object, HMM_Vec2 scale);

void BLB_Transform2D(BLB_Object2D *object, HMM_Vec2 position, float rotation, HMM_Vec2 scale);

void BLB_Object2D_SetTexture(BLB_Object2D *object, BLB_Texture *texture);

#endif
