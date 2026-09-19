#include "bulba/core/math3v/lights.h"

#include "bulba/core/math3v/HandmadeMath.h"

#include <math.h>
#include <stdlib.h>

static void BLB_ReleaseObject3DContents(BLB_Object3D *object) {
  if (!object)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  if (object->polygon) {
    free(object->polygon->vertices);
    free(object->polygon->base_vertices);
    free(object->polygon->uvs);
    free(object->polygon->indices);
    free(object->polygon);
  }

  free(object->delta_time);
}

static void BLB_ReleaseObject2DContents(BLB_Object2D *object) {
  if (!object)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  if (object->polygon) {
    free(object->polygon->vertices);
    free(object->polygon->base_vertices);
    free(object->polygon->uvs);
    free(object->polygon->indices);
    free(object->polygon);
  }

  free(object->delta_time);
}

BLB_Light3D *BLB_CreateLight3D(BLB_LightType type, HMM_Vec3 position, HMM_Vec3 rotation) {
  BLB_Light3D *light = calloc(1, sizeof(*light));

  if (!light)
    return NULL;

  light->object.position = position;
  light->object.rotation = rotation;
  light->object.scale = HMM_V3(1.0f, 1.0f, 1.0f);

  light->object.delta_time = malloc(sizeof(float));
  if (!light->object.delta_time) {
    free(light);
    return NULL;
  }

  *light->object.delta_time = 0.0f;

  light->object.color[0] = 255;
  light->object.color[1] = 255;
  light->object.color[2] = 255;
  light->object.color[3] = 255;

  light->object.visible = true;

  light->type = type;

  light->intensity = 1.0f;
  light->ambient = 0.03f;

  light->specular = 0.0f;
  light->shininess = 32.0f;

  light->range = 30.0f;

  light->inner_cone = 0.9f;
  light->outer_cone = 0.75f;

  light->enabled = true;

  return light;
}

BLB_Light2D *BLB_CreateLight2D(BLB_LightType type, HMM_Vec2 position, float rotation) {
  BLB_Light2D *light = calloc(1, sizeof(*light));

  if (!light)
    return NULL;

  light->object.position = position;
  light->object.rotation = rotation;
  light->object.scale = HMM_V2(1.0f, 1.0f);

  light->object.delta_time = malloc(sizeof(float));
  if (!light->object.delta_time) {
    free(light);
    return NULL;
  }

  *light->object.delta_time = 0.0f;

  light->object.color[0] = 255;
  light->object.color[1] = 255;
  light->object.color[2] = 255;
  light->object.color[3] = 255;

  light->object.visible = true;

  light->type = type;

  light->intensity = 1.0f;
  light->ambient = 0.03f;

  light->specular = 0.0f;
  light->shininess = 32.0f;

  light->range = 300.0f;

  light->inner_cone = 0.9f;
  light->outer_cone = 0.75f;

  light->enabled = true;

  return light;
}

void BLB_DestroyLight3D(BLB_Light3D *light) {
  if (!light)
    return;

  BLB_ReleaseObject3DContents(&light->object);

  free(light);
}

void BLB_DestroyLight2D(BLB_Light2D *light) {
  if (!light)
    return;

  BLB_ReleaseObject2DContents(&light->object);

  free(light);
}

HMM_Vec3 BLB_GetLightDirection3D(const BLB_Light3D *light) {
  if (!light)
    return HMM_V3(0.0f, 0.0f, -1.0f);

  HMM_Mat4 rx = HMM_Rotate_RH(HMM_AngleDeg(light->object.rotation.x), HMM_V3(1.0f, 0.0f, 0.0f));

  HMM_Mat4 ry = HMM_Rotate_RH(HMM_AngleDeg(light->object.rotation.y), HMM_V3(0.0f, 1.0f, 0.0f));

  HMM_Mat4 rz = HMM_Rotate_RH(HMM_AngleDeg(light->object.rotation.z), HMM_V3(0.0f, 0.0f, 1.0f));

  HMM_Mat4 rotation = HMM_MulM4(rz, HMM_MulM4(ry, rx));

  HMM_Vec4 forward = HMM_V4(0.0f, 0.0f, -1.0f, 0.0f);

  HMM_Vec4 direction = HMM_MulM4V4(rotation, forward);

  return HMM_NormV3(HMM_V3(direction.x, direction.y, direction.z));
}

HMM_Vec2 BLB_GetLightDirection2D(const BLB_Light2D *light) {
  if (!light)
    return HMM_V2(1.0f, 0.0f);

  float angle = HMM_AngleDeg(light->object.rotation);

  return HMM_NormV2(HMM_V2(cosf(angle), sinf(angle)));
}
