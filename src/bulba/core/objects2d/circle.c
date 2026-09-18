#include "bulba/core/objects2d/circle.h"

#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"

#include <math.h>
#include <stdlib.h>

static size_t circle_get_segments(int level_of_detail) {
  static const size_t lod_segments[] = {12, 16, 24, 32, 48, 64, 80, 96};

  const size_t lod_count = sizeof(lod_segments) / sizeof(lod_segments[0]);

  if (level_of_detail < 1)
    level_of_detail = 1;

  size_t index = (size_t)(level_of_detail - 1);

  if (index >= lod_count)
    index = lod_count - 1;

  return lod_segments[index];
}

BLB_Object2D *BLB_CreateCircle2D(HMM_Vec2 scale, HMM_Vec2 position, int level_of_detail, BLB_Texture *texture) {

  BLB_Object2D *object = calloc(1, sizeof(*object));
  if (object == NULL)
    return NULL;

  const size_t segments = circle_get_segments(level_of_detail);
  const size_t vertex_count = segments + 2;
  const size_t index_count = segments * 3;

  object->delta_time = calloc(1, sizeof(float));
  object->polygon = calloc(1, sizeof(*object->polygon));

  if (object->delta_time == NULL || object->polygon == NULL) {
    free(object->polygon);
    free(object->delta_time);
    free(object);
    return NULL;
  }

  object->polygon->vertices = malloc(sizeof(HMM_Vec2) * vertex_count);
  object->polygon->base_vertices = malloc(sizeof(HMM_Vec2) * vertex_count);
  object->polygon->uvs = malloc(sizeof(HMM_Vec2) * vertex_count);
  object->polygon->indices = malloc(sizeof(unsigned int) * index_count);

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

  object->polygon->vertex_count = vertex_count;
  object->polygon->index_count = index_count;

  const float radius = 0.5f;
  const HMM_Vec2 center = HMM_V2(0.0f, 0.0f);

  object->polygon->vertices[0] = center;
  object->polygon->base_vertices[0] = center;
  object->polygon->uvs[0] = HMM_V2(0.5f, 0.5f);

  for (size_t i = 0; i <= segments; i++) {
    const float angle = 2.0f * (float)M_PI * (float)i / (float)segments;

    const float x = radius * cosf(angle);
    const float y = radius * sinf(angle);

    const size_t vertex_index = 1 + i;

    const HMM_Vec2 vertex = HMM_V2(x, y);

    object->polygon->vertices[vertex_index] = vertex;
    object->polygon->base_vertices[vertex_index] = vertex;

    const float u = 0.5f + (x / (radius * 2.0f));
    const float v = 0.5f - (y / (radius * 2.0f));

    object->polygon->uvs[vertex_index] = HMM_V2(u, v);
  }

  size_t index = 0;

  for (size_t i = 0; i < segments; i++) {
    const unsigned int current = (unsigned int)(1 + i);

    const unsigned int next = (unsigned int)(1 + i + 1);
    object->polygon->indices[index++] = 0;
    object->polygon->indices[index++] = next;
    object->polygon->indices[index++] = current;
  }

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

  object->material = BLB_Material_Create3D();

  if (object->material == NULL) {
    BLB_DestroyCircle2D(object);
    return NULL;
  }

  BLB_Transform2D(object, position, object->rotation, scale);

  if (texture != NULL)
    BLB_Object2D_SetTexture(object, texture);
  else
    object->texture = NULL;

  return object;
}

void BLB_DestroyCircle2D(BLB_Object2D *object) {
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
