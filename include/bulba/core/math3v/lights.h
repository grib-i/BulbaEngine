#ifndef LIGHTS_H
#define LIGHTS_H

#include "bulba/core/math3v/math3v.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects3d/objects3d.h"

#include <stdbool.h>

typedef enum { BLB_LIGHT_POINT = 0, BLB_LIGHT_DIRECTIONAL = 1, BLB_LIGHT_SPOT = 2 } BLB_LightType;

typedef struct BLB_Light3D {
  BLB_Object3D object;

  BLB_LightType type;

  float intensity;
  float ambient;
  float specular;
  float shininess;

  float range;

  float inner_cone;
  float outer_cone;

  bool enabled;
} BLB_Light3D;

typedef struct BLB_Light2D {
  BLB_Object2D object;

  BLB_LightType type;

  float intensity;
  float ambient;
  float specular;
  float shininess;

  float range;

  float inner_cone;
  float outer_cone;

  bool enabled;
} BLB_Light2D;

BLB_Light3D *BLB_CreateLight3D(BLB_LightType type, HMM_Vec3 position, HMM_Vec3 rotation);

BLB_Light2D *BLB_CreateLight2D(BLB_LightType type, HMM_Vec2 position, float rotation);

void BLB_DestroyLight3D(BLB_Light3D *light);
void BLB_DestroyLight2D(BLB_Light2D *light);

HMM_Vec3 BLB_GetLightDirection3D(const BLB_Light3D *light);
HMM_Vec2 BLB_GetLightDirection2D(const BLB_Light2D *light);

#endif
