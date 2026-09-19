#include "bulba/core/objects2d/square.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"

#include <stdlib.h>

BLB_Object2D *BLB_CreateSquare2D(HMM_Vec2 scale, HMM_Vec2 position, BLB_Texture *texture, bool screen_space) {

  BLB_Object2D *object = calloc(1, sizeof(*object));
  if (object == NULL)
    return NULL;

  object->delta_time = calloc(1, sizeof(float));
  object->polygon = calloc(1, sizeof(*object->polygon));

  if (object->delta_time == NULL || object->polygon == NULL) {
    free(object->polygon);
    free(object->delta_time);
    free(object);
    return NULL;
  }

  object->polygon->vertices = malloc(sizeof(HMM_Vec2) * 4);
  object->polygon->base_vertices = malloc(sizeof(HMM_Vec2) * 4);
  object->polygon->uvs = malloc(sizeof(HMM_Vec2) * 4);
  object->polygon->indices = malloc(sizeof(unsigned int) * 6);

  if (object->polygon->vertices == NULL || object->polygon->base_vertices == NULL || object->polygon->uvs == NULL ||
      object->polygon->indices == NULL) {

    free(object->polygon->vertices);
    free(object->polygon->base_vertices);
    free(object->polygon->uvs);
    free(object->polygon->indices);
    free(object->polygon);
    free(object->delta_time);
    free(object);

    return NULL;
  }

  HMM_Vec2 vertices[4] = {HMM_V2(-0.5f, -0.5f), HMM_V2(0.5f, -0.5f), HMM_V2(0.5f, 0.5f), HMM_V2(-0.5f, 0.5f)};

  HMM_Vec2 uvs[4] = {HMM_V2(0.0f, 0.0f), HMM_V2(1.0f, 0.0f), HMM_V2(1.0f, 1.0f), HMM_V2(0.0f, 1.0f)};

  unsigned int indices[6] = {0, 1, 2, 0, 2, 3};

  for (size_t i = 0; i < 4; i++) {
    object->polygon->base_vertices[i] = vertices[i];
    object->polygon->vertices[i] = vertices[i];
    object->polygon->uvs[i] = uvs[i];
  }

  for (size_t i = 0; i < 6; i++)
    object->polygon->indices[i] = indices[i];

  object->polygon->vertex_count = 4;
  object->polygon->index_count = 6;

  object->position = position;
  object->rotation = 0.0f;
  object->scale = scale;

  object->render_mode = BLB_RENDER_OPAQUE;

  object->color[0] = 255;
  object->color[1] = 255;
  object->color[2] = 255;
  object->color[3] = 255;

  object->visible = true;
  object->screen_space = screen_space;
  object->entity_id = BLB_INVALID_ENTITY_ID;

  object->component_mask = BLB_COMPONENT_TRANSFORM | BLB_COMPONENT_RENDERABLE;

  object->material = BLB_Material_Create2D();

  if (!object->material) {
    BLB_DestroySquare2D(object);
    return NULL;
  }

  BLB_Object2D_Transform(object, position, object->rotation, scale);

  if (texture != NULL)
    BLB_Object2D_SetTexture(object, texture);
  else
    object->texture = NULL;

  return object;
}

void BLB_DestroySquare2D(BLB_Object2D *object) {
  if (object == NULL)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  if (object->polygon != NULL) {
    free(object->polygon->vertices);
    free(object->polygon->base_vertices);
    free(object->polygon->uvs);
    free(object->polygon->indices);
    free(object->polygon);
  }

  free(object->delta_time);
  free(object);
}
