#include "bulba/core/objects3d/cube.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"

#include <stdlib.h>
#include <time.h>

BLB_Object3D *BLB_CreateCube3D(HMM_Vec3 scale, HMM_Vec3 position, BLB_Texture *texture) {
  BLB_Object3D *object = calloc(1, sizeof(*object));
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

  object->polygon->vertices = malloc(sizeof(HMM_Vec3) * 24);
  object->polygon->base_vertices = malloc(sizeof(HMM_Vec3) * 24);
  object->polygon->vertex_count = malloc(sizeof(size_t));
  object->polygon->indices = malloc(sizeof(unsigned int) * 36);

  object->polygon->uvs = malloc(sizeof(HMM_Vec2) * 24);
  object->mesh.uvs = malloc(sizeof(HMM_Vec2) * 24);

  object->mesh.vertices = malloc(sizeof(HMM_Vec3) * 24);
  object->mesh.indices = malloc(sizeof(unsigned int) * 36);
  object->base_vertices = malloc(sizeof(HMM_Vec3) * 24);

  if (object->polygon->vertices == NULL || object->polygon->base_vertices == NULL || object->polygon->vertex_count == NULL ||
      object->polygon->indices == NULL || object->mesh.vertices == NULL || object->mesh.indices == NULL || object->base_vertices == NULL ||
      object->polygon->uvs == NULL || object->mesh.uvs == NULL) {
    free(object->polygon->vertices);
    free(object->polygon->base_vertices);
    free(object->polygon->vertex_count);
    free(object->polygon->indices);
    free(object->mesh.uvs);
    free(object->polygon->uvs);
    free(object->polygon);
    free(object->mesh.vertices);
    free(object->mesh.indices);
    free(object->base_vertices);
    free(object->delta_time);
    free(object);
    return NULL;
  }

  HMM_Vec3 vertices[24] = {HMM_V3(-0.5f, -0.5f, -0.5f), HMM_V3(0.5f, -0.5f, -0.5f),  HMM_V3(0.5f, 0.5f, -0.5f),  HMM_V3(-0.5f, 0.5f, -0.5f),
                           HMM_V3(0.5f, -0.5f, -0.5f),  HMM_V3(0.5f, -0.5f, 0.5f),   HMM_V3(0.5f, 0.5f, 0.5f),   HMM_V3(0.5f, 0.5f, -0.5f),
                           HMM_V3(0.5f, -0.5f, 0.5f),   HMM_V3(-0.5f, -0.5f, 0.5f),  HMM_V3(-0.5f, 0.5f, 0.5f),  HMM_V3(0.5f, 0.5f, 0.5f),
                           HMM_V3(-0.5f, -0.5f, 0.5f),  HMM_V3(-0.5f, -0.5f, -0.5f), HMM_V3(-0.5f, 0.5f, -0.5f), HMM_V3(-0.5f, 0.5f, 0.5f),
                           HMM_V3(-0.5f, 0.5f, -0.5f),  HMM_V3(0.5f, 0.5f, -0.5f),   HMM_V3(0.5f, 0.5f, 0.5f),   HMM_V3(-0.5f, 0.5f, 0.5f),
                           HMM_V3(-0.5f, -0.5f, 0.5f),  HMM_V3(0.5f, -0.5f, 0.5f),   HMM_V3(0.5f, -0.5f, -0.5f), HMM_V3(-0.5f, -0.5f, -0.5f)};

  unsigned int indices[36] = {0,  3,  2,  0,  2,  1,  4,  7,  6,  4,  6,  5,  8,  10, 9,  8,  11, 10,
                              12, 15, 14, 12, 14, 13, 16, 19, 18, 16, 18, 17, 20, 23, 22, 20, 22, 21};

  for (size_t i = 0; i < 24; i++) {
    object->polygon->base_vertices[i] = vertices[i];
    object->base_vertices[i] = vertices[i];
    object->polygon->vertices[i] = vertices[i];
    object->mesh.vertices[i] = vertices[i];
  }

  for (size_t i = 0; i < 36; i++) {
    object->polygon->indices[i] = indices[i];
    object->mesh.indices[i] = indices[i];
  }

  HMM_Vec2 face_uvs[4] = {HMM_V2(0.0f, 1.0f), HMM_V2(1.0f, 1.0f), HMM_V2(1.0f, 0.0f), HMM_V2(0.0f, 0.0f)};
  for (size_t face = 0; face < 6; face++) {
    for (size_t corner = 0; corner < 4; corner++) {
      size_t index = face * 4 + corner;

      object->polygon->uvs[index] = face_uvs[corner];
      object->mesh.uvs[index] = face_uvs[corner];
    }
  }

  *object->polygon->vertex_count = 24;
  object->polygon->index_count = 36;
  object->mesh.vertex_count = 24;
  object->mesh.index_count = 36;

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
  if (!object->material) {
    BLB_DestroyCube3D(object);
    return NULL;
  }

  BLB_Transform(object, position, object->rotation, scale);

  if (texture != NULL) {
    BLB_Object3D_SetTexture(object, texture);
  } else {
    object->texture = NULL;
  }

  return object;
}

void BLB_DestroyCube3D(BLB_Object3D *object) {

  if (object == NULL)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  free(object->polygon->uvs);
  free(object->mesh.uvs);

  if (object->polygon != NULL) {
    free(object->polygon->vertices);
    free(object->polygon->indices);
    free(object->polygon->vertex_count);
    free(object->polygon->base_vertices);
    free(object->polygon);
  }

  free(object->mesh.vertices);
  free(object->mesh.indices);
  free(object->base_vertices);
  free(object->delta_time);
  free(object);
}
