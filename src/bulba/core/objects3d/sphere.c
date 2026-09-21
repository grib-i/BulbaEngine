#include "bulba/core/objects3d/sphere.h"
#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/utils/object.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static BLB_Polygon3D *polygon = NULL;

static void BLB_SphereFreePolygon(void) {
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

static size_t sphere_get_slices(int level_of_detail) {
  static const size_t lod_slices[] = {12, 16, 24, 32, 48, 64, 80, 96};

  const size_t lod_count = sizeof(lod_slices) / sizeof(lod_slices[0]);

  if (level_of_detail < 1)
    level_of_detail = 1;

  size_t index = (size_t)(level_of_detail - 1);

  if (index >= lod_count)
    index = lod_count - 1;

  return lod_slices[index];
}

BLB_Object3D *BLB_CreateSphere3D(HMM_Vec3 scale, HMM_Vec3 position, int level_of_detail, BLB_Texture *texture) {

  BLB_Object3D *object = calloc(1, sizeof(*object));

  if (object == NULL)
    return NULL;

  bool polygon_created = false;

  object->delta_time = calloc(1, sizeof(*object->delta_time));

  if (object->delta_time == NULL)
    goto fail;

  object->type = BLB_OBJECT_SPHERE;

  if (BLB_OBJECTS_ID == NULL || object->type >= BLB_OBJECTS_ID_COUNT)
    goto fail;

  object->id = &BLB_OBJECTS_ID[object->type];

  if (polygon == NULL && object->id->id == 0 && strcmp(object->id->id_type, "sphere") == 0) {

    polygon = calloc(1, sizeof(*object->polygon));

    if (polygon == NULL)
      goto fail;

    polygon_created = true;

    const size_t slices = sphere_get_slices(level_of_detail);

    const size_t stacks = slices / 2;

    const size_t vertex_count = 2 + (stacks - 1) * (slices + 1);

    const size_t index_count = 6 * slices * (stacks - 1);

    polygon->vertices = malloc(sizeof(HMM_Vec3) * vertex_count);

    polygon->base_vertices = malloc(sizeof(HMM_Vec3) * vertex_count);

    polygon->uvs = malloc(sizeof(HMM_Vec2) * vertex_count);

    polygon->indices = malloc(sizeof(unsigned int) * index_count);

    if (polygon->vertices == NULL || polygon->base_vertices == NULL || polygon->uvs == NULL || polygon->indices == NULL)
      goto fail;

    float *sin_theta = malloc(sizeof(float) * (slices + 1));

    float *cos_theta = malloc(sizeof(float) * (slices + 1));

    float *sin_phi = malloc(sizeof(float) * (stacks + 1));

    float *cos_phi = malloc(sizeof(float) * (stacks + 1));

    if (sin_theta == NULL || cos_theta == NULL || sin_phi == NULL || cos_phi == NULL) {

      free(sin_theta);
      free(cos_theta);
      free(sin_phi);
      free(cos_phi);

      goto fail;
    }

    for (size_t j = 0; j <= slices; j++) {
      const float theta = 2.0f * (float)M_PI * (float)j / (float)slices;

      sin_theta[j] = sinf(theta);
      cos_theta[j] = cosf(theta);
    }

    for (size_t i = 0; i <= stacks; i++) {
      const float phi = (float)M_PI * (float)i / (float)stacks;

      sin_phi[i] = sinf(phi);
      cos_phi[i] = cosf(phi);
    }

    size_t vertex_index = 0;

    const HMM_Vec3 top = HMM_V3(0.0f, 0.5f, 0.0f);

    polygon->vertices[vertex_index] = top;
    polygon->base_vertices[vertex_index] = top;
    polygon->uvs[vertex_index] = HMM_V2(0.5f, 0.0f);

    vertex_index++;

    for (size_t i = 1; i < stacks; i++) {
      const float sin_current_phi = sin_phi[i];

      const float cos_current_phi = cos_phi[i];

      const float v = (float)i / (float)stacks;

      for (size_t j = 0; j <= slices; j++) {
        HMM_Vec3 vertex = HMM_V3(0.5f * sin_current_phi * cos_theta[j], 0.5f * cos_current_phi, 0.5f * sin_current_phi * sin_theta[j]);

        polygon->vertices[vertex_index] = vertex;

        polygon->base_vertices[vertex_index] = vertex;

        polygon->uvs[vertex_index] = HMM_V2((float)j / (float)slices, v);

        vertex_index++;
      }
    }

    const size_t bottom_index = vertex_index;

    const HMM_Vec3 bottom = HMM_V3(0.0f, -0.5f, 0.0f);

    polygon->vertices[bottom_index] = bottom;

    polygon->base_vertices[bottom_index] = bottom;

    polygon->uvs[bottom_index] = HMM_V2(0.5f, 1.0f);

    size_t index = 0;

    for (size_t j = 0; j < slices; j++) {
      const size_t current = 1 + j;
      const size_t next = current + 1;

      polygon->indices[index++] = 0;
      polygon->indices[index++] = (unsigned int)next;
      polygon->indices[index++] = (unsigned int)current;
    }

    for (size_t i = 0; i < stacks - 2; i++) {
      const size_t row = 1 + i * (slices + 1);

      const size_t next_row = row + slices + 1;

      for (size_t j = 0; j < slices; j++) {
        const size_t a = row + j;
        const size_t b = row + j + 1;
        const size_t c = next_row + j;
        const size_t d = next_row + j + 1;

        polygon->indices[index++] = (unsigned int)a;

        polygon->indices[index++] = (unsigned int)b;

        polygon->indices[index++] = (unsigned int)c;

        polygon->indices[index++] = (unsigned int)b;

        polygon->indices[index++] = (unsigned int)d;

        polygon->indices[index++] = (unsigned int)c;
      }
    }

    const size_t bottom_row = 1 + (stacks - 2) * (slices + 1);

    for (size_t j = 0; j < slices; j++) {
      const size_t current = bottom_row + j;

      const size_t next = current + 1;

      polygon->indices[index++] = (unsigned int)current;

      polygon->indices[index++] = (unsigned int)next;

      polygon->indices[index++] = (unsigned int)bottom_index;
    }

    free(sin_theta);
    free(cos_theta);
    free(sin_phi);
    free(cos_phi);

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

  object->mesh.vertices = object->polygon->vertices;

  object->mesh.normals = NULL;

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
    BLB_DestroySphere3D(object);
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
    BLB_SphereFreePolygon();

  free(object->delta_time);
  free(object);

  return NULL;
}

void BLB_DestroySphere3D(BLB_Object3D *object) {
  if (object == NULL)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  if (object->id != NULL && object->id->id > 0) {

    object->id->id--;

    if (object->id->id == 0)
      BLB_SphereFreePolygon();
  }

  free(object->delta_time);
  free(object);
}
