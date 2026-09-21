#include "bulba/core/objects3d/torus.h"

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

static void BLB_TorusFreePolygon(void) {
  if (polygon == NULL)
    return;

  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->normals);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon);

  polygon = NULL;
}

static size_t torus_get_slices(int level_of_detail) {
  static const size_t lod_slices[] = {12, 16, 24, 32, 48, 64, 80, 96};
  const size_t lod_count = sizeof(lod_slices) / sizeof(lod_slices[0]);

  if (level_of_detail < 1)
    level_of_detail = 1;

  size_t index = (size_t)(level_of_detail - 1);

  if (index >= lod_count)
    index = lod_count - 1;

  return lod_slices[index];
}

BLB_Object3D *BLB_CreateTorus3D(
    HMM_Vec3 scale,
    HMM_Vec3 position,
    int level_of_detail,
    float outer_radius,
    float hole_radius,
    BLB_Texture *texture) {
  BLB_Object3D *object = calloc(1, sizeof(*object));

  if (object == NULL)
    return NULL;

  bool polygon_created = false;

  if (level_of_detail < 1)
    level_of_detail = 1;

  if (outer_radius <= 0.0f ||
      hole_radius <= 0.0f ||
      outer_radius <= hole_radius) {
    outer_radius = 0.3f;
    hole_radius = 0.1f;
  }

  const float major_radius = (outer_radius + hole_radius) * 0.5f;
  const float minor_radius = (outer_radius - hole_radius) * 0.5f;

  if (major_radius <= 0.000001f ||
      minor_radius <= 0.000001f)
    goto fail;

  object->delta_time = calloc(1, sizeof(*object->delta_time));

  if (object->delta_time == NULL)
    goto fail;

  object->type = BLB_OBJECT_TORUS;

  if (BLB_OBJECTS_ID == NULL ||
      object->type >= BLB_OBJECTS_ID_COUNT)
    goto fail;

  object->id = &BLB_OBJECTS_ID[object->type];

  if (polygon == NULL &&
      object->id->id == 0 &&
      strcmp(object->id->id_type, "torus") == 0) {
    polygon = calloc(1, sizeof(*polygon));

    if (polygon == NULL)
      goto fail;

    polygon_created = true;

    const size_t slices = torus_get_slices(level_of_detail);
    const size_t stacks = slices / 2;
    const size_t vertex_count = (stacks + 1) * (slices + 1);
    const size_t index_count = 6 * slices * stacks;

    polygon->vertices =
        malloc(sizeof(*polygon->vertices) * vertex_count);

    polygon->base_vertices =
        malloc(sizeof(*polygon->base_vertices) * vertex_count);

    polygon->normals =
        malloc(sizeof(*polygon->normals) * vertex_count);

    polygon->uvs =
        malloc(sizeof(*polygon->uvs) * vertex_count);

    polygon->indices =
        malloc(sizeof(*polygon->indices) * index_count);

    if (polygon->vertices == NULL ||
        polygon->base_vertices == NULL ||
        polygon->normals == NULL ||
        polygon->uvs == NULL ||
        polygon->indices == NULL)
      goto fail;

    const float tau = 6.28318530717958647692f;
    size_t vertex_index = 0;

    for (size_t i = 0; i <= stacks; i++) {
      const float phi = tau * (float)i / (float)stacks;
      const float sin_phi = sinf(phi);
      const float cos_phi = cosf(phi);
      const float v = (float)i / (float)stacks;

      for (size_t j = 0; j <= slices; j++) {
        const float theta = tau * (float)j / (float)slices;
        const float sin_theta = sinf(theta);
        const float cos_theta = cosf(theta);

        const float radial =
            major_radius + minor_radius * cos_phi;

        HMM_Vec3 vertex = HMM_V3(
            radial * cos_theta,
            minor_radius * sin_phi,
            radial * sin_theta
        );

        HMM_Vec3 normal = HMM_V3(
            cos_phi * cos_theta,
            sin_phi,
            cos_phi * sin_theta
        );

        polygon->vertices[vertex_index] = vertex;
        polygon->base_vertices[vertex_index] = vertex;
        polygon->normals[vertex_index] = HMM_NormV3(normal);
        polygon->uvs[vertex_index] =
            HMM_V2(
                (float)j / (float)slices,
                v
            );

        vertex_index++;
      }
    }

    size_t index = 0;

    for (size_t i = 0; i < stacks; i++) {
      const size_t row = i * (slices + 1);
      const size_t next_row = (i + 1) * (slices + 1);

      for (size_t j = 0; j < slices; j++) {
        const size_t a = row + j;
        const size_t b = row + j + 1;
        const size_t c = next_row + j;
        const size_t d = next_row + j + 1;

        polygon->indices[index++] = (unsigned int)a;
        polygon->indices[index++] = (unsigned int)c;
        polygon->indices[index++] = (unsigned int)b;
        polygon->indices[index++] = (unsigned int)b;
        polygon->indices[index++] = (unsigned int)c;
        polygon->indices[index++] = (unsigned int)d;
      }
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

  if (object->delta_time == NULL ||
      object->polygon == NULL)
    goto fail;

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

  object->component_mask =
      BLB_COMPONENT_TRANSFORM |
      BLB_COMPONENT_RENDERABLE;

  object->material = BLB_Material_Create3D();

  if (object->material == NULL) {
    BLB_DestroyTorus3D(object);
    return NULL;
  }

  const float normalization =
      outer_radius / minor_radius -
      hole_radius / major_radius;

  scale =
      HMM_MulV3F(
          scale,
          normalization
      );

  BLB_Object3D_Transform(
      object,
      position,
      object->rotation,
      scale
  );

  if (texture != NULL)
    BLB_Object3D_SetTexture(object, texture);
  else
    object->texture = NULL;

  return object;

fail:
  if (polygon_created)
    BLB_TorusFreePolygon();

  free(object->delta_time);
  free(object);

  return NULL;
}

void BLB_DestroyTorus3D(BLB_Object3D *object) {
  if (object == NULL)
    return;

  if (object->material != NULL)
    BLB_Material_Release(object->material);

  if (object->texture != NULL)
    BLB_Texture_Release(object->texture);

  if (object->id != NULL &&
      object->id->id > 0) {
    object->id->id--;

    if (object->id->id == 0)
      BLB_TorusFreePolygon();
  }

  free(object->delta_time);
  free(object);
}
