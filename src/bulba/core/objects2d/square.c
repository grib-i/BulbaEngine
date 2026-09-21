#include "bulba/core/objects2d/square.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/utils/object.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static BLB_Polygon2D *polygon = NULL;

static void BLB_SquareFreePolygon(void) {
  if (polygon == NULL)
    return;

  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon);

  polygon = NULL;
}

BLB_Object2D *BLB_CreateSquare2D(HMM_Vec2 scale, HMM_Vec2 position, BLB_Texture *texture, bool screen_space) {

  BLB_Object2D *object = calloc(1, sizeof(*object));

  if (object == NULL)
    return NULL;

  bool polygon_created = false;

  object->delta_time = calloc(1, sizeof(*object->delta_time));

  if (object->delta_time == NULL)
    goto fail;

  object->type = BLB_OBJECT_SQUARE;

  if (BLB_OBJECTS_ID == NULL || object->type >= BLB_OBJECTS_ID_COUNT)
    goto fail;

  object->id = &BLB_OBJECTS_ID[object->type];

  if (polygon == NULL && object->id->id == 0 && strcmp(object->id->id_type, "square") == 0) {

    polygon = calloc(1, sizeof(*object->polygon));

    if (polygon == NULL)
      goto fail;

    polygon_created = true;

    polygon->vertices = malloc(sizeof(HMM_Vec2) * 4);

    polygon->base_vertices = malloc(sizeof(HMM_Vec2) * 4);

    polygon->uvs = malloc(sizeof(HMM_Vec2) * 4);

    polygon->indices = malloc(sizeof(unsigned int) * 6);

    if (polygon->vertices == NULL || polygon->base_vertices == NULL || polygon->uvs == NULL || polygon->indices == NULL)
      goto fail;

    HMM_Vec2 vertices[4] = {HMM_V2(-0.5f, -0.5f), HMM_V2(0.5f, -0.5f), HMM_V2(0.5f, 0.5f), HMM_V2(-0.5f, 0.5f)};

    HMM_Vec2 uvs[4] = {HMM_V2(0.0f, 0.0f), HMM_V2(1.0f, 0.0f), HMM_V2(1.0f, 1.0f), HMM_V2(0.0f, 1.0f)};

    unsigned int indices[6] = {0, 1, 2, 0, 2, 3};

    for (size_t i = 0; i < 4; i++) {
      polygon->vertices[i] = vertices[i];

      polygon->base_vertices[i] = vertices[i];

      polygon->uvs[i] = uvs[i];
    }

    for (size_t i = 0; i < 6; i++)
      polygon->indices[i] = indices[i];

    polygon->vertex_count = 4;
    polygon->index_count = 6;

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
    BLB_DestroySquare2D(object);
    return NULL;
  }

  BLB_Object2D_Transform(object, position, object->rotation, scale);

  if (texture != NULL)
    BLB_Object2D_SetTexture(object, texture);
  else
    object->texture = NULL;

  return object;

fail:
  if (polygon_created)
    BLB_SquareFreePolygon();

  free(object->delta_time);
  free(object);

  return NULL;
}

void BLB_DestroySquare2D(BLB_Object2D *object) {
  if (object == NULL)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  if (object->id != NULL && object->id->id > 0) {

    object->id->id--;

    if (object->id->id == 0)
      BLB_SquareFreePolygon();
  }

  free(object->delta_time);
  free(object);
}
