#include "bulba/core/objects3d/cube.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/utils/object.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static BLB_Polygon3D *polygon = NULL;

static void BLB_CubeFreePolygon(void) {
  if (polygon == NULL)
    return;

  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon->normals);
  free(polygon);

  polygon = NULL;
}

BLB_Object3D *BLB_CreateCube3D(HMM_Vec3 scale, HMM_Vec3 position, BLB_Texture *texture) {

  BLB_Object3D *object = calloc(1, sizeof(*object));

  if (object == NULL)
    return NULL;

  bool polygon_created = false;

  object->delta_time = calloc(1, sizeof(*object->delta_time));

  if (object->delta_time == NULL)
    goto fail;

  object->type = BLB_OBJECT_CUBE;

  if (BLB_OBJECTS_ID == NULL || object->type >= BLB_OBJECTS_ID_COUNT)
    goto fail;

  object->id = &BLB_OBJECTS_ID[object->type];

  if (polygon == NULL && object->id->id == 0 && strcmp(object->id->id_type, "cube") == 0) {

    polygon = calloc(1, sizeof(*object->polygon));

    if (polygon == NULL)
      goto fail;

    polygon_created = true;

    polygon->vertices = malloc(sizeof(HMM_Vec3) * 24);

    polygon->base_vertices = malloc(sizeof(HMM_Vec3) * 24);

    polygon->uvs = malloc(sizeof(HMM_Vec2) * 24);

    polygon->indices = malloc(sizeof(unsigned int) * 36);

    if (polygon->vertices == NULL || polygon->base_vertices == NULL || polygon->uvs == NULL || polygon->indices == NULL)
      goto fail;

    HMM_Vec3 vertices[24] = {HMM_V3(-0.5f, -0.5f, -0.5f), HMM_V3(0.5f, -0.5f, -0.5f),  HMM_V3(0.5f, 0.5f, -0.5f),  HMM_V3(-0.5f, 0.5f, -0.5f),

                             HMM_V3(0.5f, -0.5f, -0.5f),  HMM_V3(0.5f, -0.5f, 0.5f),   HMM_V3(0.5f, 0.5f, 0.5f),   HMM_V3(0.5f, 0.5f, -0.5f),

                             HMM_V3(0.5f, -0.5f, 0.5f),   HMM_V3(-0.5f, -0.5f, 0.5f),  HMM_V3(-0.5f, 0.5f, 0.5f),  HMM_V3(0.5f, 0.5f, 0.5f),

                             HMM_V3(-0.5f, -0.5f, 0.5f),  HMM_V3(-0.5f, -0.5f, -0.5f), HMM_V3(-0.5f, 0.5f, -0.5f), HMM_V3(-0.5f, 0.5f, 0.5f),

                             HMM_V3(-0.5f, 0.5f, -0.5f),  HMM_V3(0.5f, 0.5f, -0.5f),   HMM_V3(0.5f, 0.5f, 0.5f),   HMM_V3(-0.5f, 0.5f, 0.5f),

                             HMM_V3(-0.5f, -0.5f, 0.5f),  HMM_V3(0.5f, -0.5f, 0.5f),   HMM_V3(0.5f, -0.5f, -0.5f), HMM_V3(-0.5f, -0.5f, -0.5f)};

    unsigned int indices[36] = {0,  3,  2,  0,  2,  1,  4,  7,  6,  4,  6,  5,  8,  10, 9,  8,  11, 10,
                                12, 15, 14, 12, 14, 13, 16, 19, 18, 16, 18, 17, 20, 23, 22, 20, 22, 21};

    HMM_Vec2 face_uvs[4] = {HMM_V2(0.0f, 0.0f), HMM_V2(1.0f, 0.0f), HMM_V2(1.0f, 1.0f), HMM_V2(0.0f, 1.0f)};

    for (size_t i = 0; i < 24; i++) {
      polygon->vertices[i] = vertices[i];
      polygon->base_vertices[i] = vertices[i];
    }

    for (size_t face = 0; face < 6; face++) {
      for (size_t corner = 0; corner < 4; corner++) {
        const size_t i = face * 4 + corner;
        polygon->uvs[i] = face_uvs[corner];
      }
    }

    for (size_t i = 0; i < 36; i++)
      polygon->indices[i] = indices[i];

    polygon->vertex_count = 24;
    polygon->index_count = 36;

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

  object->mesh.vertices = object->polygon->vertices;
  object->mesh.normals = object->polygon->normals;
  object->mesh.uvs = object->polygon->uvs;
  object->mesh.vertex_count = object->polygon->vertex_count;
  object->mesh.indices = object->polygon->indices;
  object->mesh.index_count = object->polygon->index_count;

  object->position = position;
  object->rotation = HMM_V3(0.0f, 0.0f, 0.0f);
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
    BLB_DestroyCube3D(object);
    return NULL;
  }

  BLB_Object3D_Transform(object, position, object->rotation, scale);

  if (texture != NULL)
    BLB_Object3D_SetTexture(object, texture);
  else
    object->texture = NULL;

  return object;

fail:
  if (polygon_created)
    BLB_CubeFreePolygon();

  free(object->delta_time);
  free(object);

  return NULL;
}

void BLB_DestroyCube3D(BLB_Object3D *object) {
  if (object == NULL)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  if (object->id != NULL && object->id->id > 0) {

    object->id->id--;

    if (object->id->id == 0)
      BLB_CubeFreePolygon();
  }

  free(object->delta_time);
  free(object);
}
