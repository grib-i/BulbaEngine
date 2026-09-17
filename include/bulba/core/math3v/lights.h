#ifndef LIGHTS_H
#define LIGHTS_H

#include "bulba/core/math3v/math3v.h"
#include "bulba/core/objects3d/objects3d.h"

#include <stdbool.h>

typedef enum { BLB_LIGHT_POINT = 0, BLB_LIGHT_DIRECTIONAL = 1, BLB_LIGHT_SPOT = 2 } BLB_LightType;

typedef struct BLB_Light3D {
  HMM_Vec3 position;
  HMM_Vec3 direction;
  HMM_Vec3 color;
  float intensity;
  float ambient;
  float specular;
  float shininess;
  float range;
  float inner_cone;
  float outer_cone;
  BLB_LightType type;
  bool enabled;
  BLB_Object3D *owner;
  HMM_Vec3 owner_local_position;
  HMM_Vec3 owner_local_direction;
} BLB_Light3D;


typedef struct BLB_SuperObject3D {
  BLB_Object3D object;
  BLB_Light3D light;
} BLB_SuperObject3D;

typedef BLB_SuperObject3D BLB_LightObject3D;

typedef struct BLB_Light2D {
  HMM_Vec2 position;
  HMM_Vec2 direction;
  HMM_Vec3 color;
  float intensity;
  float ambient;
  float specular;
  float shininess;
  float range;
  float inner_cone;
  float outer_cone;
  BLB_LightType type;
  bool enabled;
} BLB_Light2D;

BLB_Light3D *BLB_CreateLight3D(BLB_LightType type, HMM_Vec3 position, HMM_Vec3 direction);
BLB_Light2D *BLB_CreateLight2D(BLB_LightType type, HMM_Vec2 position, HMM_Vec2 direction);

void BLB_DestroyLight3D(BLB_Light3D *light);
void BLB_DestroyLight2D(BLB_Light2D *light);

void BLB_AttachLight3D(BLB_Light3D *light, BLB_Object3D *owner, HMM_Vec3 local_position, HMM_Vec3 local_direction);
void BLB_DetachLight3D(BLB_Light3D *light);
void BLB_UpdateLight3D(BLB_Light3D *light);

BLB_SuperObject3D *BLB_CreateSuperLightObject3D(BLB_LightType type, HMM_Vec3 scale, HMM_Vec3 position, HMM_Vec3 direction);
void BLB_DestroySuperLightObject3D(BLB_SuperObject3D *object);

#endif
