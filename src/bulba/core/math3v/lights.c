#include "bulba/core/math3v/lights.h"

#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/objects3d/cube.h"
#include "bulba/core/render/material.h"

#include <stdlib.h>
#include <time.h>

BLB_Light3D *BLB_CreateLight3D(BLB_LightType type, HMM_Vec3 position, HMM_Vec3 direction) {
  BLB_Light3D *light = calloc(1, sizeof(*light));

  if (!light)
    return NULL;

  light->type = type;
  light->position = position;
  light->direction = HMM_NormV3(direction);
  light->color = HMM_V3(1.0f, 1.0f, 1.0f);
  light->intensity = 1.0f;
  light->ambient = 0.03f;
  light->specular = 0.0f;
  light->shininess = 32.0f;
  light->range = 30.0f;
  light->inner_cone = 0.9f;
  light->outer_cone = 0.75f;
  light->enabled = true;
  light->owner = NULL;
  light->owner_local_position = HMM_V3(0.0f, 0.0f, 0.0f);
  light->owner_local_direction = light->direction;

  return light;
}

BLB_Light2D *BLB_CreateLight2D(BLB_LightType type, HMM_Vec2 position, HMM_Vec2 direction) {
  BLB_Light2D *light = calloc(1, sizeof(*light));

  if (!light)
    return NULL;

  light->type = type;
  light->position = position;
  light->direction = direction;
  light->color = HMM_V3(1.0f, 1.0f, 1.0f);
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

void BLB_DestroyLight3D(BLB_Light3D *light) { free(light); }

void BLB_DestroyLight2D(BLB_Light2D *light) { free(light); }

void BLB_AttachLight3D(BLB_Light3D *light, BLB_Object3D *owner, HMM_Vec3 local_position, HMM_Vec3 local_direction) {
  if (!light)
    return;

  light->owner = owner;
  light->owner_local_position = local_position;
  light->owner_local_direction = HMM_NormV3(local_direction);
  BLB_UpdateLight3D(light);
}

void BLB_DetachLight3D(BLB_Light3D *light) {
  if (!light)
    return;

  light->owner = NULL;
}

void BLB_UpdateLight3D(BLB_Light3D *light) {
  if (!light || !light->owner)
    return;

  BLB_Object3D *owner = light->owner;
  HMM_Mat4 rx = HMM_Rotate_RH(HMM_AngleDeg(owner->rotation.x), HMM_V3(1.0f, 0.0f, 0.0f));
  HMM_Mat4 ry = HMM_Rotate_RH(HMM_AngleDeg(owner->rotation.y), HMM_V3(0.0f, 1.0f, 0.0f));
  HMM_Mat4 rz = HMM_Rotate_RH(HMM_AngleDeg(owner->rotation.z), HMM_V3(0.0f, 0.0f, 1.0f));
  HMM_Mat4 rotation = HMM_MulM4(rz, HMM_MulM4(ry, rx));

  HMM_Vec4 local_position = HMM_V4(light->owner_local_position.x, light->owner_local_position.y, light->owner_local_position.z, 1.0f);
  HMM_Vec4 local_direction = HMM_V4(light->owner_local_direction.x, light->owner_local_direction.y, light->owner_local_direction.z, 0.0f);
  HMM_Vec4 world_position = HMM_MulM4V4(rotation, local_position);
  HMM_Vec4 world_direction = HMM_MulM4V4(rotation, local_direction);

  light->position =
      HMM_AddV3(owner->position, HMM_V3(world_position.x * owner->scale.x, world_position.y * owner->scale.y, world_position.z * owner->scale.z));

  if (light->type == BLB_LIGHT_DIRECTIONAL || light->type == BLB_LIGHT_SPOT) {
    light->direction = HMM_NormV3(HMM_V3(world_direction.x, world_direction.y, world_direction.z));
  }
}

BLB_SuperObject3D *BLB_CreateSuperLightObject3D(BLB_LightType type, HMM_Vec3 scale, HMM_Vec3 position, HMM_Vec3 direction) {
  BLB_Object3D *base = BLB_CreateCube3D(scale, position, NULL);
  if (!base)
    return NULL;

  BLB_SuperObject3D *super = calloc(1, sizeof(*super));
  if (!super) {
    BLB_DestroyCube3D(base);
    return NULL;
  }

  super->object = *base;
  free(base);

  BLB_Light3D *light = BLB_CreateLight3D(type, position, direction);
  if (!light) {
    if (super->object.material)
      BLB_Material_Release(super->object.material);

    if (super->object.polygon) {
      free(super->object.polygon->vertices);
      free(super->object.polygon->indices);
      free(super->object.polygon->vertex_count);
      free(super->object.polygon->base_vertices);
      free(super->object.polygon->uvs);
      free(super->object.polygon);
    }

    free(super->object.mesh.vertices);
    free(super->object.mesh.indices);
    free(super->object.mesh.uvs);
    free(super->object.base_vertices);
    free(super->object.delta_time);
    free(super);

    return NULL;
  }

  super->light = *light;
  free(light);
  BLB_AttachLight3D(&super->light, &super->object, HMM_V3(0.0f, 0.0f, 0.0f), direction);
  super->object.visible = false;

  return super;
}

void BLB_DestroySuperLightObject3D(BLB_SuperObject3D *object) {
  if (!object)
    return;

  object->light.owner = NULL;

  if (object->object.material)
    BLB_Material_Release(object->object.material);

  if (object->object.polygon) {
    free(object->object.polygon->vertices);
    free(object->object.polygon->indices);
    free(object->object.polygon->vertex_count);
    free(object->object.polygon->base_vertices);
    free(object->object.polygon->uvs);
    free(object->object.polygon);
  }

  free(object->object.mesh.vertices);
  free(object->object.mesh.indices);
  free(object->object.mesh.uvs);
  free(object->object.base_vertices);
  free(object->object.delta_time);
  free(object);
}
