#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/physics/physics2d/physics2d.h"
#include "bulba/core/render/material.h"

#include <math.h>
#include <stdlib.h>

void BLB_ClearAnimation(BLB_Object2D *object) {
  if (!object || !object->animation)
    return;

  BLB_Animation *animation = object->animation;

  for (size_t i = 0; i < animation->textures_count; ++i)
    BLB_Texture_Release(animation->textures[i]);

  free(animation->textures);
  *animation = (BLB_Animation){0};
}

void BLB_SetAnimation(BLB_Object2D *object, BLB_Texture **textures, float frame_time, size_t textures_count) {
  if (!object || !object->animation || textures_count > SIZE_MAX / sizeof(*textures))
    return;

  if (!textures || !textures_count) {
    BLB_ClearAnimation(object);

    return;
  }

  BLB_Texture **frames = malloc(textures_count * sizeof(*frames));

  if (!frames)
    return;

  for (size_t i = 0; i < textures_count; ++i) {
    if (!textures[i] || textures[i]->type != BLB_TEXTURE_2D) {
      for (size_t j = 0; j < i; ++j)
        BLB_Texture_Release(frames[j]);
      free(frames);

      return;
    }
    frames[i] = textures[i];
    BLB_Texture_Retain(frames[i]);
  }
  BLB_ClearAnimation(object);
  object->animation->textures = frames;
  object->animation->textures_count = textures_count;
  object->animation->frame_time = isfinite(frame_time) && frame_time > 0.0f ? frame_time : 0.0f;
}

BLB_Texture *BLB_Object2D_GetTexture(const BLB_Object2D *object) {
  if (!object)
    return NULL;
  const BLB_Animation *animation = object->animation;

  if (animation && animation->textures && animation->textures_count)
    return animation->textures[animation->texture_counter % animation->textures_count];

  return object->texture;
}

void BLB_StartAnimation(BLB_Object2D *object) {
  if (object && object->animation && object->animation->textures_count && object->animation->frame_time > 0.0f)
    object->animation->enable = true;
}

void BLB_StopAnimation(BLB_Object2D *object) {
  if (object && object->animation)
    object->animation->enable = false;
}

void BLB_Object2D_UpdateAnimation(BLB_Object2D *object, float delta_time) {
  BLB_Animation *animation = object ? object->animation : NULL;

  if (!animation || !animation->enable || !animation->textures || !animation->textures_count || !isfinite(delta_time) || delta_time < 0.0f ||
      !isfinite(animation->frame_time) || animation->frame_time <= 0.0f)
    return;

  double elapsed = (double)animation->frame_count + delta_time;
  double steps = floor(elapsed / animation->frame_time);
  size_t advance = (size_t)fmod(steps, (double)animation->textures_count);
  size_t current = animation->texture_counter % animation->textures_count;
  size_t remaining = animation->textures_count - current;

  animation->texture_counter = advance >= remaining ? advance - remaining : current + advance;
  animation->frame_count = (float)fmod(elapsed, animation->frame_time);
}

static void apply_position(BLB_Object2D *object, BLB_Vec2 value, BLB_TransformType operation) {
  if (!object || !isfinite(value.x) || !isfinite(value.y))
    return;

  object->position = value;

  if (object->rigid_body && BLB_PhysicsSyncToBody(object->physics_sync_mode[operation])) {
    b2Body_SetTransform(object->rigid_body->handle, (b2Vec2){value.x, value.y}, b2Body_GetRotation(object->rigid_body->handle));
  }
}

void BLB_Object2D_Move(BLB_Object2D *object, BLB_Vec2 velocity) {
  if (!object)
    return;

  if (object->delta_time)
    velocity = BLB_MulV2F(velocity, object->delta_time);

  apply_position(object, BLB_AddV2(object->position, velocity), BLB_TRANSFORM_TYPE_MOVE);
}

void BLB_Object2D_SetPosition(BLB_Object2D *object, BLB_Vec2 value) { apply_position(object, value, BLB_TRANSFORM_TYPE_SET_POSITION); }

static void apply_rotation(BLB_Object2D *object, float value, BLB_TransformType operation) {
  if (!object || !isfinite(value))
    return;

  object->rotation = value;

  if (object->rigid_body && BLB_PhysicsSyncToBody(object->physics_sync_mode[operation])) {
    b2Body_SetTransform(object->rigid_body->handle, b2Body_GetPosition(object->rigid_body->handle), b2MakeRot(value * (float)BLB_PI / 180.0f));
  }
}

void BLB_Object2D_Rotate(BLB_Object2D *object, float velocity) {
  if (!object)
    return;

  if (object->delta_time)
    velocity = velocity * object->delta_time;

  apply_rotation(object, object->rotation + velocity, BLB_TRANSFORM_TYPE_ROTATE);
}

void BLB_Object2D_SetRotation(BLB_Object2D *object, float value) { apply_rotation(object, value, BLB_TRANSFORM_TYPE_SET_ROTATION); }

static void apply_scale(BLB_Object2D *object, BLB_Vec2 value, BLB_TransformType operation) {
  if (!object || !isfinite(value.x) || !isfinite(value.y))
    return;

  object->scale = value;

  if (object->rigid_body && BLB_PhysicsSyncToBody(object->physics_sync_mode[operation])) {
    BLB_Physics2D_ColliderSetScale(object, value);
  }
}

void BLB_Object2D_Scale(BLB_Object2D *object, BLB_Vec2 velocity) {
  if (!object)
    return;

  if (object->delta_time)
    velocity = BLB_MulV2F(velocity, object->delta_time);

  apply_scale(object, BLB_AddV2(object->scale, velocity), BLB_TRANSFORM_TYPE_SCALE);
}

void BLB_Object2D_SetScale(BLB_Object2D *object, BLB_Vec2 value) { apply_scale(object, value, BLB_TRANSFORM_TYPE_SET_SCALE); }

void BLB_Object2D_Transform(BLB_Object2D *object, BLB_Vec2 position, float rotation, BLB_Vec2 scale) {
  apply_position(object, position, BLB_TRANSFORM_TYPE_TRANSFORM);
  apply_rotation(object, rotation, BLB_TRANSFORM_TYPE_TRANSFORM);
  apply_scale(object, scale, BLB_TRANSFORM_TYPE_TRANSFORM);
}

void blb_object2d_sync_from_body(BLB_Object2D *object) {
  if (!object || !object->rigid_body)
    return;

  BLB_Vec2 position = object->position;
  float rotation = object->rotation;
  BLB_Vec2 scale = object->scale;

  if (BLB_PhysicsSyncToObject(object->physics_sync_mode[BLB_TRANSFORM_TYPE_SET_POSITION]))
    position = BLB_Physics2D_BodyGetPosition(object);

  if (BLB_PhysicsSyncToObject(object->physics_sync_mode[BLB_TRANSFORM_TYPE_SET_ROTATION]))
    rotation = BLB_Physics2D_BodyGetRotation(object) * 180.0f / (float)BLB_PI;

  if (object->collider && BLB_PhysicsSyncToObject(object->physics_sync_mode[BLB_TRANSFORM_TYPE_SET_SCALE]))
    scale = object->collider->sync_scale;

  object->position = position;
  object->rotation = rotation;
  object->scale = scale;
}

void blb_object2d_sync_to_body(BLB_Object2D *object) {
  if (!object || !object->rigid_body)
    return;

  if (object->physics_sync_mode[BLB_TRANSFORM_TYPE_SET_POSITION] == BLB_PHYSICS_SYNC_OBJECT_TO_BODY)
    apply_position(object, object->position, BLB_TRANSFORM_TYPE_SET_POSITION);

  if (object->physics_sync_mode[BLB_TRANSFORM_TYPE_SET_ROTATION] == BLB_PHYSICS_SYNC_OBJECT_TO_BODY)
    apply_rotation(object, object->rotation, BLB_TRANSFORM_TYPE_SET_ROTATION);

  if (object->physics_sync_mode[BLB_TRANSFORM_TYPE_SET_SCALE] == BLB_PHYSICS_SYNC_OBJECT_TO_BODY)
    apply_scale(object, object->scale, BLB_TRANSFORM_TYPE_SET_SCALE);
}

void BLB_Object2D_SetCameraAttached(BLB_Object2D *object, bool attached) {
  if (object)
    object->camera_attached = attached;
}

bool BLB_Object2D_IsCameraAttached(const BLB_Object2D *object) { return object && object->camera_attached; }

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
  object->handler->down_buttons = 0;
  object->handler->pressed_buttons = 0;
  object->handler->released_buttons = 0;
  object->handler->hovered = false;
  object->handler->enabled = true;
}

void BLB_Object2D_ClearHandler(BLB_Object2D *object) {
  if (!object)
    return;
  free(object->handler);
  object->handler = NULL;
}

void BLB_Object2D_SetPhysicsSyncMode(BLB_Object2D *object, BLB_PhysicsSyncMode mode, BLB_TransformType operation) {
  if (!object || operation < 0 || operation >= BLB_TRANSFORM_TYPE_COUNT || mode < BLB_PHYSICS_SYNC_NONE || mode > BLB_PHYSICS_SYNC_BIDIRECTIONAL)
    return;

  if (operation == BLB_TRANSFORM_TYPE_TRANSFORM) {
    for (int i = 0; i < BLB_TRANSFORM_TYPE_COUNT; ++i)
      object->physics_sync_mode[i] = mode;
  } else {
    object->physics_sync_mode[operation] = mode;
  }
}

void BLB_Object2D_SetColor(BLB_Object2D *object, BLB_Vec4 color) {
  if (!object)
    return;

  object->color = color;
}
