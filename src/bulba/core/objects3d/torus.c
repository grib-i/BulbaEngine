#include "bulba/core/objects3d/torus.h"

#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/utils/object.h"

#define _GNU_SOURCE
#include <math.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TORUS_CACHE_INITIAL_CAPACITY 8u
#define TORUS_KEY_SLICE_BITS 16u

/*
 * A torus mesh is determined by (major, minor, LOD).
 * We build one mesh per unique geometry variant and let all objects share it.
 * The 64-bit key intentionally contains both (major + minor) and (major * minor),
 * plus the slice count. Comparing the original floats as well protects against
 * the tiny chance of a packed-key collision.
 */
typedef struct {
  uint64_t geometry_id;
  float major_radius;
  float minor_radius;
  size_t slices;
  size_t refs;
  BLB_Polygon3D *polygon;
} BLB_TorusGeometry;

static BLB_TorusGeometry *torus_cache = NULL;
static size_t torus_cache_count = 0;
static size_t torus_cache_capacity = 0;

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

static uint32_t float_bits(float value) {
  uint32_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));
  return bits;
}

static uint64_t hash64_mix(uint64_t x) {
  x ^= x >> 30u;
  x *= UINT64_C(0xbf58476d1ce4e5b9);
  x ^= x >> 27u;
  x *= UINT64_C(0x94d049bb133111eb);
  x ^= x >> 31u;
  return x;
}

static uint64_t torus_geometry_id(float major_radius, float minor_radius, size_t slices) {
  const float radius_sum = major_radius + minor_radius;
  const float radius_product = major_radius * minor_radius;

  uint64_t key = ((uint64_t)float_bits(radius_sum) << 32u) | float_bits(radius_product);
  key ^= ((uint64_t)(slices & ((1u << TORUS_KEY_SLICE_BITS) - 1u))) << 16u;
  key ^= UINT64_C(0x544f5255535f4745);
  uint64_t hash = hash64_mix(key);
  return hash != 0 ? hash : UINT64_C(1);
}

static void free_torus_polygon(BLB_Polygon3D *polygon) {
  if (!polygon)
    return;

  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->normals);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon);
}

static BLB_Polygon3D *create_torus_polygon(float major_radius, float minor_radius, size_t slices) {
  const size_t stacks = slices / 2u;
  const size_t vertex_count = (stacks + 1u) * (slices + 1u);
  const size_t index_count = 6u * slices * stacks;

  BLB_Polygon3D *polygon = calloc(1, sizeof(*polygon));
  if (!polygon)
    return NULL;

  polygon->vertices = malloc(sizeof(*polygon->vertices) * vertex_count);
  polygon->base_vertices = malloc(sizeof(*polygon->base_vertices) * vertex_count);
  polygon->normals = malloc(sizeof(*polygon->normals) * vertex_count);
  polygon->uvs = malloc(sizeof(*polygon->uvs) * vertex_count);
  polygon->indices = malloc(sizeof(*polygon->indices) * index_count);

  if (!polygon->vertices || !polygon->base_vertices || !polygon->normals || !polygon->uvs || !polygon->indices) {
    free_torus_polygon(polygon);
    return NULL;
  }

  const float tau = 6.28318530717958647692f;
  size_t vertex_index = 0;

  for (size_t i = 0; i <= stacks; ++i) {
    const float phi = tau * (float)i / (float)stacks;
    const float sin_phi = sinf(phi);
    const float cos_phi = cosf(phi);
    const float v = (float)i / (float)stacks;

    for (size_t j = 0; j <= slices; ++j) {
      const float theta = tau * (float)j / (float)slices;
      const float sin_theta = sinf(theta);
      const float cos_theta = cosf(theta);
      const float radial = major_radius + minor_radius * cos_phi;

      const HMM_Vec3 vertex = HMM_V3(radial * cos_theta, minor_radius * sin_phi, radial * sin_theta);
      const HMM_Vec3 normal = HMM_NormV3(HMM_V3(cos_phi * cos_theta, sin_phi, cos_phi * sin_theta));

      polygon->vertices[vertex_index] = vertex;
      polygon->base_vertices[vertex_index] = vertex;
      polygon->normals[vertex_index] = normal;
      polygon->uvs[vertex_index] = HMM_V2((float)j / (float)slices, v);
      ++vertex_index;
    }
  }

  size_t index = 0;
  for (size_t i = 0; i < stacks; ++i) {
    const size_t row = i * (slices + 1u);
    const size_t next_row = (i + 1u) * (slices + 1u);

    for (size_t j = 0; j < slices; ++j) {
      const unsigned int a = (unsigned int)(row + j);
      const unsigned int b = (unsigned int)(row + j + 1u);
      const unsigned int c = (unsigned int)(next_row + j);
      const unsigned int d = (unsigned int)(next_row + j + 1u);

      polygon->indices[index++] = a;
      polygon->indices[index++] = c;
      polygon->indices[index++] = b;
      polygon->indices[index++] = b;
      polygon->indices[index++] = c;
      polygon->indices[index++] = d;
    }
  }

  polygon->vertex_count = vertex_count;
  polygon->index_count = index_count;
  return polygon;
}

static bool torus_cache_reserve(size_t required) {
  if (torus_cache_capacity >= required)
    return true;

  size_t capacity = torus_cache_capacity ? torus_cache_capacity : TORUS_CACHE_INITIAL_CAPACITY;
  while (capacity < required) {
    if (capacity > SIZE_MAX / 2u)
      return false;
    capacity *= 2u;
  }

  BLB_TorusGeometry *new_cache = realloc(torus_cache, capacity * sizeof(*new_cache));
  if (!new_cache)
    return false;

  torus_cache = new_cache;
  torus_cache_capacity = capacity;
  return true;
}

static bool torus_geometry_matches(const BLB_TorusGeometry *entry, uint64_t id, float major_radius, float minor_radius, size_t slices) {
  if (!entry || entry->geometry_id != id || entry->slices != slices)
    return false;

  return memcmp(&entry->major_radius, &major_radius, sizeof(float)) == 0 && memcmp(&entry->minor_radius, &minor_radius, sizeof(float)) == 0;
}

static BLB_TorusGeometry *torus_cache_acquire(float major_radius, float minor_radius, size_t slices) {
  const uint64_t geometry_id = torus_geometry_id(major_radius, minor_radius, slices);

  for (size_t i = 0; i < torus_cache_count; ++i) {
    if (torus_geometry_matches(&torus_cache[i], geometry_id, major_radius, minor_radius, slices)) {
      torus_cache[i].refs++;
      return &torus_cache[i];
    }
  }

  if (!torus_cache_reserve(torus_cache_count + 1u))
    return NULL;

  BLB_Polygon3D *polygon = create_torus_polygon(major_radius, minor_radius, slices);
  if (!polygon)
    return NULL;

  BLB_TorusGeometry *entry = &torus_cache[torus_cache_count++];
  *entry = (BLB_TorusGeometry){
      .geometry_id = geometry_id,
      .major_radius = major_radius,
      .minor_radius = minor_radius,
      .slices = slices,
      .refs = 1,
      .polygon = polygon,
  };

  return entry;
}

static void torus_cache_release(BLB_Object3D *object) {
  if (!object || !object->polygon || object->geometry_id == 0)
    return;

  for (size_t i = 0; i < torus_cache_count; ++i) {
    BLB_TorusGeometry *entry = &torus_cache[i];
    if (entry->geometry_id != object->geometry_id || entry->polygon != object->polygon)
      continue;

    if (entry->refs > 0)
      --entry->refs;

    if (entry->refs != 0)
      return;

    free_torus_polygon(entry->polygon);

    const size_t last = torus_cache_count - 1u;
    if (i != last)
      torus_cache[i] = torus_cache[last];
    --torus_cache_count;
    return;
  }
}

BLB_Object3D *BLB_CreateTorus3D(HMM_Vec3 scale, HMM_Vec3 position, int level_of_detail, float outer_radius, float hole_radius, BLB_Texture *texture) {
  BLB_Object3D *object = calloc(1, sizeof(*object));
  if (!object)
    return NULL;

  if (level_of_detail < 1)
    level_of_detail = 1;

  if (outer_radius <= 0.0f || hole_radius <= 0.0f || outer_radius <= hole_radius) {
    outer_radius = 0.3f;
    hole_radius = 0.1f;
  }

  const float major_radius = (outer_radius + hole_radius) * 0.5f;
  const float minor_radius = (outer_radius - hole_radius) * 0.5f;
  const size_t slices = torus_get_slices(level_of_detail);

  if (major_radius <= 0.000001f || minor_radius <= 0.000001f)
    goto fail;

  object->delta_time = calloc(1, sizeof(*object->delta_time));
  if (!object->delta_time)
    goto fail;

  object->type = BLB_OBJECT_TORUS;
  if (!BLB_OBJECTS_ID || object->type >= BLB_OBJECTS_ID_COUNT)
    goto fail;

  object->id = &BLB_OBJECTS_ID[object->type];
  if (object->id->id == BLB_INVALID_OBJECT_ID)
    goto fail;

  BLB_TorusGeometry *geometry = torus_cache_acquire(major_radius, minor_radius, slices);
  if (!geometry)
    goto fail;

  object->geometry_id = geometry->geometry_id;
  object->polygon = geometry->polygon;
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
  if (!object->material) {
    torus_cache_release(object);
    object->geometry_id = 0;
    object->polygon = NULL;
    object->id->id--;
    goto fail;
  }

  const float normalization = outer_radius / minor_radius - hole_radius / major_radius;
  BLB_Object3D_Transform(object, position, object->rotation, HMM_MulV3F(scale, normalization));

  if (texture)
    BLB_Object3D_SetTexture(object, texture);

  return object;

fail:
  free(object->delta_time);
  free(object);
  return NULL;
}

void BLB_DestroyTorus3D(BLB_Object3D *object) {
  if (!object)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  torus_cache_release(object);

  if (object->id && object->id->id > 0)
    object->id->id--;

  object->polygon = NULL;
  object->geometry_id = 0;

  free(object->delta_time);
  free(object);
}
