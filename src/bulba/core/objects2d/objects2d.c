#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/render/material.h"

#include <stdlib.h>

void BLB_SetAnimation(BLB_Object2D *object, BLB_Texture **textures, float frame_time, size_t textures_count) {
  object->animation->textures = textures;
  object->animation->frame_time = frame_time;
  object->animation->enable = false;
  object->animation->texture_counter = 0;
  object->animation->frame_count = 0;
  object->animation->textures_count = textures_count;
}

void BLB_StartAnimation(BLB_Object2D *object) { object->animation->enable = true; }

void BLB_StopAnimation(BLB_Object2D *object) { object->animation->enable = false; }

void BLB_Object2D_Move(BLB_Object2D *object, HMM_Vec2 velocity) {
  if (!object)
    return;

  if (object->delta_time)
    velocity = HMM_MulV2F(velocity, *object->delta_time);

  object->position = HMM_AddV2(object->position, velocity);
}

void BLB_Object2D_SetPosition(BLB_Object2D *object, HMM_Vec2 position) {
  if (!object)
    return;

  object->position = position;
}

void BLB_Object2D_Rotate(BLB_Object2D *object, float velocity) {
  if (!object)
    return;

  if (object->delta_time)
    velocity *= *object->delta_time;

  object->rotation += velocity;
}

void BLB_Object2D_SetRotation(BLB_Object2D *object, float rotation) {
  if (!object)
    return;

  object->rotation = rotation;
}

void BLB_Object2D_Scale(BLB_Object2D *object, HMM_Vec2 velocity) {
  if (!object)
    return;

  if (object->delta_time)
    velocity = HMM_MulV2F(velocity, *object->delta_time);

  object->scale = HMM_AddV2(object->scale, velocity);
}

void BLB_Object2D_SetScale(BLB_Object2D *object, HMM_Vec2 scale) {
  if (!object)
    return;

  object->scale = scale;
}

void BLB_Object2D_Transform(BLB_Object2D *object, HMM_Vec2 position, float rotation, HMM_Vec2 scale) {
  if (!object)
    return;

  object->position = position;
  object->rotation = rotation;
  object->scale = scale;
}

void BLB_Object2D_SetMaterial(BLB_Object2D *object, BLB_Material *material) {
  if (!object || object->material == material)
    return;

  if (material)
    BLB_Material_Retain(material);

  if (object->material)
    BLB_Material_Release(object->material);

  object->material = material;
}

int BLB_Object2D_SetMaterialFromFile(BLB_Object2D *object, const char *path) {
  if (!object || !path || !path[0])
    return -1;

  BLB_Material *material = BLB_Material_Load(path, NULL, NULL, NULL, NULL);
  if (!material)
    return -1;

  if (material->domain != BLB_MATERIAL_2D) {
    BLB_Material_Release(material);
    return -1;
  }

  BLB_Object2D_SetMaterial(object, material);
  BLB_Material_Release(material);

  return 0;
}

int BLB_Object2D_SetTexture(BLB_Object2D *object, BLB_Texture *texture) {
  if (!object)
    return -1;

  if (texture && texture->type == BLB_TEXTURE_3D)
    return -1;

  if (object->texture == texture)
    return 0;

  if (texture)
    BLB_Texture_Retain(texture);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  object->texture = texture;
  return 0;
}

void BLB_Object2D_FlipX(BLB_Object2D *object) {
  if (!object)
    return;

  object->scale.x *= -1.0f;
}

void BLB_Object2D_FlipY(BLB_Object2D *object) {
  if (!object)
    return;

  object->scale.y *= -1.0f;
}

void BLB_Object2D_SetHandler(BLB_Object2D *object, const BLB_ObjectHandler *handler) {
  if (!object)
    return;

  if (!handler) {
    BLB_Object2D_ClearHandler(object);
    return;
  }

  if (!object->handler) {
    object->handler = malloc(sizeof(*object->handler));
    if (!object->handler)
      return;
  }

  *object->handler = *handler;
  object->handler->captured_buttons = 0;
  object->handler->hovered = false;
  object->handler->enabled = true;
}

void BLB_Object2D_ClearHandler(BLB_Object2D *object) {
  if (!object)
    return;
  free(object->handler);
  object->handler = NULL;
}
