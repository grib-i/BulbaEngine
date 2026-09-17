#include "bulba/core/objects2d/cube.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/render/material.h"

#include <stdlib.h>

BLB_Object2D *BLB_CreateCube2D(HMM_Vec2 scale, HMM_Vec2 position, BLB_Texture *texture) {
  BLB_Object2D *object = calloc(1, sizeof(*object));
  if (object == NULL)
    return NULL;

  object->delta_time = calloc(1, sizeof(float));
  if (object->delta_time == NULL) {
    free(object);
    return NULL;
  }

  object->polygon = calloc(1, sizeof(*object->polygon));
  if (object->polygon == NULL) {
    free(object->delta_time);
    free(object);
    return NULL;
  }

  object->polygon->vertices = malloc(sizeof(HMM_Vec2) * 4);
  object->polygon->base_vertices = malloc(sizeof(HMM_Vec2) * 4);
  object->polygon->vertex_count = malloc(sizeof(size_t));
  object->polygon->indices = malloc(sizeof(unsigned int) * 6);
  object->polygon->uvs = malloc(sizeof(HMM_Vec2) * 4);

  if (object->polygon->vertices == NULL || object->polygon->base_vertices == NULL || object->polygon->vertex_count == NULL ||
      object->polygon->indices == NULL || object->polygon->uvs == NULL) {
    free(object->polygon->vertices);
    free(object->polygon->base_vertices);
    free(object->polygon->vertex_count);
    free(object->polygon->indices);
    free(object->polygon->uvs);
    free(object->polygon);
    free(object->delta_time);
    free(object);
    return NULL;
  }

  object->polygon->base_vertices[0] = HMM_V2(-0.5f, -0.5f);
  object->polygon->base_vertices[1] = HMM_V2(0.5f, -0.5f);
  object->polygon->base_vertices[2] = HMM_V2(0.5f, 0.5f);
  object->polygon->base_vertices[3] = HMM_V2(-0.5f, 0.5f);

  for (size_t i = 0; i < 4; i++)
    object->polygon->vertices[i] = object->polygon->base_vertices[i];

  object->polygon->uvs[0] = HMM_V2(0.0f, 0.0f);
  object->polygon->uvs[1] = HMM_V2(1.0f, 0.0f);
  object->polygon->uvs[2] = HMM_V2(1.0f, 1.0f);
  object->polygon->uvs[3] = HMM_V2(0.0f, 1.0f);

  object->polygon->indices[0] = 0;
  object->polygon->indices[1] = 1;
  object->polygon->indices[2] = 2;
  object->polygon->indices[3] = 0;
  object->polygon->indices[4] = 2;
  object->polygon->indices[5] = 3;
  object->polygon->index_count = 6;
  *object->polygon->vertex_count = 4;

  object->position = position;
  object->rotation = 0.0f;
  object->scale = scale;
  object->render_mode = BLB_RENDER_OPAQUE;
  object->color[0] = 255;
  object->color[1] = 255;
  object->color[2] = 255;
  object->color[3] = 255;
  object->visible = true;
  object->entity_id = BLB_INVALID_ENTITY_ID;
  object->component_mask = BLB_COMPONENT_TRANSFORM | BLB_COMPONENT_RENDERABLE;
  object->material = BLB_Material_Create2D();
  if (!object->material) {
    BLB_DestroyCube2D(object);
    return NULL;
  }

  BLB_Transform2D(object, position, object->rotation, scale);

  if (texture != NULL) {
    BLB_Object2D_SetTexture(object, texture);
  } else {
    object->texture = NULL;
  }

  return object;
}

void BLB_DestroyCube2D(BLB_Object2D *object) {
  if (object == NULL)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  free(object->polygon->uvs);

  if (object->polygon != NULL) {
    free(object->polygon->vertices);
    free(object->polygon->indices);
    free(object->polygon->vertex_count);
    free(object->polygon->base_vertices);
    free(object->polygon);
  }

  free(object->delta_time);
  free(object);
}
