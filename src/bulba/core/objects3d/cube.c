#include "bulba/core/objects3d/cube.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/physics/physics3d/physics3d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/object_lod.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/utils/delta_time.h"
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

BLB_Object3D *BLB_CreateCube3D(BLB_Vec3 scale, BLB_Vec3 position, BLB_Texture *texture, BLB_TextureProjection mapping) {
  BLB_Object3D *object = calloc(1, sizeof(*object));

  if (object == NULL)
    return NULL;

  bool polygon_created = false;

  object->delta_time = BLB_DeltaTime_Get();

  object->type = BLB_OBJECT_CUBE;

  if (BLB_OBJECTS_ID == NULL || object->type >= BLB_OBJECTS_ID_COUNT)
    goto fail;

  object->id = &BLB_OBJECTS_ID[object->type];

  if (polygon == NULL && object->id->id == 0 && strcmp(object->id->id_type, "cube") == 0) {

    polygon = calloc(1, sizeof(*object->polygon));

    if (polygon == NULL)
      goto fail;

    polygon_created = true;

    polygon->vertices = malloc(sizeof(BLB_Vec3) * 24);
    polygon->base_vertices = malloc(sizeof(BLB_Vec3) * 24);
    polygon->normals = malloc(sizeof(BLB_Vec3) * 24);
    polygon->uvs = malloc(sizeof(BLB_Vec2) * 24);
    polygon->indices = malloc(sizeof(unsigned int) * 36);

    if (polygon->vertices == NULL || polygon->base_vertices == NULL || polygon->normals == NULL || polygon->uvs == NULL || polygon->indices == NULL)
      goto fail;

    BLB_Vec3 vertices[24] = {BLB_V3(-0.5f, -0.5f, -0.5f), BLB_V3(0.5f, -0.5f, -0.5f),  BLB_V3(0.5f, 0.5f, -0.5f),  BLB_V3(-0.5f, 0.5f, -0.5f),

                             BLB_V3(0.5f, -0.5f, -0.5f),  BLB_V3(0.5f, -0.5f, 0.5f),   BLB_V3(0.5f, 0.5f, 0.5f),   BLB_V3(0.5f, 0.5f, -0.5f),

                             BLB_V3(0.5f, -0.5f, 0.5f),   BLB_V3(-0.5f, -0.5f, 0.5f),  BLB_V3(-0.5f, 0.5f, 0.5f),  BLB_V3(0.5f, 0.5f, 0.5f),

                             BLB_V3(-0.5f, -0.5f, 0.5f),  BLB_V3(-0.5f, -0.5f, -0.5f), BLB_V3(-0.5f, 0.5f, -0.5f), BLB_V3(-0.5f, 0.5f, 0.5f),

                             BLB_V3(-0.5f, 0.5f, -0.5f),  BLB_V3(0.5f, 0.5f, -0.5f),   BLB_V3(0.5f, 0.5f, 0.5f),   BLB_V3(-0.5f, 0.5f, 0.5f),

                             BLB_V3(-0.5f, -0.5f, 0.5f),  BLB_V3(0.5f, -0.5f, 0.5f),   BLB_V3(0.5f, -0.5f, -0.5f), BLB_V3(-0.5f, -0.5f, -0.5f)};

    unsigned int indices[36] = {0,  3,  2,  0,  2,  1,  4,  7,  6,  4,  6,  5,  8,  10, 9,  8,  11, 10,
                                12, 15, 14, 12, 14, 13, 16, 19, 18, 16, 18, 17, 20, 23, 22, 20, 22, 21};
    BLB_Vec2 face_uvs[6][4] = {// BACK (-Z)
                               {BLB_V2(1.00f, 0.333333f), BLB_V2(0.75f, 0.333333f), BLB_V2(0.75f, 0.666667f), BLB_V2(1.00f, 0.666667f)},
                               // RIGHT (+X)
                               {BLB_V2(0.75f, 0.333333f), BLB_V2(0.50f, 0.333333f), BLB_V2(0.50f, 0.666667f), BLB_V2(0.75f, 0.666667f)},
                               // FRONT (+Z)
                               {BLB_V2(0.50f, 0.333333f), BLB_V2(0.25f, 0.333333f), BLB_V2(0.25f, 0.666667f), BLB_V2(0.50f, 0.666667f)},
                               // LEFT (-X)
                               {BLB_V2(0.25f, 0.333333f), BLB_V2(0.00f, 0.333333f), BLB_V2(0.00f, 0.666667f), BLB_V2(0.25f, 0.666667f)},
                               // TOP (+Y)
                               {BLB_V2(0.25f, 1.000000f), BLB_V2(0.50f, 1.000000f), BLB_V2(0.50f, 0.666667f), BLB_V2(0.25f, 0.666667f)},
                               // BOTTOM (-Y)
                               {BLB_V2(0.25f, 0.333333f), BLB_V2(0.50f, 0.333333f), BLB_V2(0.50f, 0.000000f), BLB_V2(0.25f, 0.000000f)}};

    for (size_t face = 0; face < 6; face++) {
      for (size_t corner = 0; corner < 4; corner++) {
        polygon->uvs[face * 4 + corner] = face_uvs[face][corner];
      }
    }

    const BLB_Vec3 face_normals[6] = {BLB_V3(0.0f, 0.0f, -1.0f), BLB_V3(1.0f, 0.0f, 0.0f), BLB_V3(0.0f, 0.0f, 1.0f),
                                      BLB_V3(-1.0f, 0.0f, 0.0f), BLB_V3(0.0f, 1.0f, 0.0f), BLB_V3(0.0f, -1.0f, 0.0f)};

    for (size_t i = 0; i < 24; i++) {
      polygon->vertices[i] = vertices[i];
      polygon->base_vertices[i] = vertices[i];
      polygon->normals[i] = face_normals[i / 4u];
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

  object->polygon = polygon;

  object->id->id++;

  object->mesh.vertices = object->polygon->vertices;
  object->mesh.normals = object->polygon->normals;
  object->mesh.uvs = object->polygon->uvs;
  object->mesh.vertex_count = object->polygon->vertex_count;
  object->mesh.indices = object->polygon->indices;
  object->mesh.index_count = object->polygon->index_count;

  object->position = position;
  object->rotation = BLB_V3(0.0f, 0.0f, 0.0f);
  object->scale = scale;
  object->render_mode = BLB_RENDER_OPAQUE;
  object->color = BLB_V4(255.0f, 255.0f, 255.0f, 255.0f);
  object->visible = true;
  object->entity_id = BLB_INVALID_ENTITY_ID;
  object->component_mask = BLB_COMPONENT_TRANSFORM | BLB_COMPONENT_RENDERABLE;

  for (int i = 0; i < BLB_TRANSFORM_TYPE_COUNT; i++) {
    object->physics_sync_mode[i] = BLB_PHYSICS_SYNC_BIDIRECTIONAL;
  }

  object->material = BLB_Material_Create3D();

  if (object->material == NULL) {
    BLB_DestroyCube3D(object);

    return NULL;
  }

  BLB_Object3D_Transform(object, position, object->rotation, scale);

  if (texture != NULL) {
    BLB_Object3D_SetTexture(object, texture);

    if (object->material)
      object->material->base_color_texture.projection = mapping;
  } else {
    object->texture = NULL;
  }

  return object;
fail:
  if (polygon_created)
    BLB_CubeFreePolygon();

  free(object);

  return NULL;
}

void BLB_DestroyCube3D(BLB_Object3D *object) {
  if (object == NULL)
    return;

  if (object->rigid_body)
    BLB_Physics3D_BodyDestroy(object->rigid_body->world, object);

  BLB_Object3D_ClearLODs(object);

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  if (object->id != NULL && object->id->id > 0) {

    object->id->id--;

    if (object->id->id == 0)
      BLB_CubeFreePolygon();
  }

  free(object->handler);
  free(object);
}
