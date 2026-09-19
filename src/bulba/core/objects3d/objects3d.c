#include "bulba/core/objects3d/objects3d.h"

void BLB_Object3D_Move(BLB_Object3D *object, HMM_Vec3 velocity) {
  if (!object)
    return;

  if (object->delta_time)
    velocity = HMM_MulV3F(velocity, *object->delta_time);

  object->position = HMM_AddV3(object->position, velocity);
}

void BLB_Object3D_SetPosition(BLB_Object3D *object, HMM_Vec3 position) {
  if (!object)
    return;

  object->position = position;
}

void BLB_Object3D_Rotate(BLB_Object3D *object, HMM_Vec3 angular_velocity) {
  if (!object)
    return;

  if (object->delta_time)
    angular_velocity = HMM_MulV3F(angular_velocity, *object->delta_time);

  object->rotation = HMM_AddV3(object->rotation, angular_velocity);
}

void BLB_Object3D_SetRotation(BLB_Object3D *object, HMM_Vec3 rotation) {
  if (!object)
    return;

  object->rotation = rotation;
}

void BLB_Object3D_Scale(BLB_Object3D *object, HMM_Vec3 scale_velocity) {
  if (!object)
    return;

  if (object->delta_time)
    scale_velocity = HMM_MulV3F(scale_velocity, *object->delta_time);

  object->scale = HMM_AddV3(object->scale, scale_velocity);
}

void BLB_Object3D_SetScale(BLB_Object3D *object, HMM_Vec3 scale) {
  if (!object)
    return;

  object->scale = scale;
}

void BLB_Object3D_Transform(BLB_Object3D *object, HMM_Vec3 position, HMM_Vec3 rotation, HMM_Vec3 scale) {
  if (!object)
    return;

  object->position = position;
  object->rotation = rotation;
  object->scale = scale;
}

void BLB_Object3D_SetTexture(BLB_Object3D *object, BLB_Texture *texture) {
  if (!object)
    return;

  if (object->texture == texture)
    return;

  if (texture)
    BLB_Texture_Retain(texture);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  object->texture = texture;
}
