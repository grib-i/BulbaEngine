#include "bulba/core/objects2d/circle.h"

#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/utils/object.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static BLB_Polygon2D *polygon = NULL;

static void BLB_CircleFreePolygon(void) {
  if (polygon == NULL)
    return;

  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon);

  polygon = NULL;
}

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

BLB_Object2D *BLB_CreateCircle2D(HMM_Vec2 scale, HMM_Vec2 position, int level_of_detail, BLB_Texture *texture, bool screen_space) {
  BLB_Object2D *object = calloc(1, sizeof(*object));

  if (object == NULL)
    return NULL;

  bool polygon_created = false;

  object->delta_time = calloc(1, sizeof(*object->delta_time));

  if (object->delta_time == NULL)
    goto fail;

  object->type = BLB_OBJECT_CIRCLE2D;

  if (BLB_OBJECTS_ID == NULL || object->type >= BLB_OBJECTS_ID_COUNT)
    goto fail;

  object->id = &BLB_OBJECTS_ID[object->type];

  if (polygon == NULL && object->id->id == 0 && strcmp(object->id->id_type, "circle2d") == 0) {
    polygon = calloc(1, sizeof(*object->polygon));

    if (polygon == NULL)
      goto fail;

    polygon_created = true;

    const size_t segments = circle_get_segments(level_of_detail);
    const size_t vertex_count = segments + 2;
    const size_t index_count = segments * 3;
    polygon->vertices = malloc(sizeof(HMM_Vec2) * vertex_count);
    polygon->base_vertices = malloc(sizeof(HMM_Vec2) * vertex_count);
    polygon->uvs = malloc(sizeof(HMM_Vec2) * vertex_count);
    polygon->indices = malloc(sizeof(unsigned int) * index_count);

    if (polygon->vertices == NULL || polygon->base_vertices == NULL || polygon->uvs == NULL || polygon->indices == NULL)
      goto fail;

    const float radius = 0.5f;

    const HMM_Vec2 center = HMM_V2(0.0f, 0.0f);

    polygon->vertices[0] = center;
    polygon->base_vertices[0] = center;
    polygon->uvs[0] = HMM_V2(0.5f, 0.5f);

    for (size_t i = 0; i <= segments; i++) {

      const float angle = 2.0f * (float)M_PI * (float)i / (float)segments;
      const float x = radius * cosf(angle);
      const float y = radius * sinf(angle);
      const size_t vertex_index = 1 + i;
      const HMM_Vec2 vertex = HMM_V2(x, y);

      polygon->vertices[vertex_index] = vertex;

      polygon->base_vertices[vertex_index] = vertex;

      const float u = 0.5f + x / (radius * 2.0f);

      const float v = 0.5f - y / (radius * 2.0f);

      polygon->uvs[vertex_index] = HMM_V2(u, v);
    }

    size_t index = 0;

    for (size_t i = 0; i < segments; i++) {

      const unsigned int current = (unsigned int)(1 + i);

      const unsigned int next = (unsigned int)(1 + i + 1);

      polygon->indices[index++] = 0;
      polygon->indices[index++] = next;
      polygon->indices[index++] = current;
    }

    polygon->vertex_count = vertex_count;

    polygon->index_count = index_count;

  } else if (object->id->id == BLB_INVALID_OBJECT_ID) {
    goto fail;

  } else {
    object->polygon = polygon;
  }

  if (object->polygon == NULL)
    object->polygon = polygon;

  if (object->delta_time == NULL || object->polygon == NULL)
    goto fail;

  object->polygon = polygon;

  object->id->id++;

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

  if (object->material == NULL) {
    BLB_DestroyCircle2D(object);
    return NULL;
  }

  object->animation = calloc(1, sizeof(*object->animation));
  if (object->animation == NULL)
    goto fail;

  BLB_Object2D_Transform(object, position, object->rotation, scale);

  if (texture != NULL)
    BLB_Object2D_SetTexture(object, texture);
  else
    object->texture = NULL;

  return object;

fail:
  if (polygon_created)
    BLB_CircleFreePolygon();

  free(object->delta_time);
  free(object);

  return NULL;
}

void BLB_DestroyCircle2D(BLB_Object2D *object) {
  if (object == NULL)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  if (object->id != NULL && object->id->id > 0) {

    object->id->id--;

    if (object->id->id == 0)
      BLB_CircleFreePolygon();
  }

  free(object->animation);
  free(object->delta_time);
  free(object);
}
