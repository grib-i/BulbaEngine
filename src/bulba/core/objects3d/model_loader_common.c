#include "bulba/core/objects3d/model_loader_common.h"

#include <stdlib.h>
#include <string.h>

BLB_ModelGeometry *model_cache = NULL;
size_t model_cache_count = 0;
size_t model_cache_capacity = 0;

bool BLB_ModelArrayReserve(void **data, size_t *capacity, size_t required, size_t element_size) {
  if (*capacity >= required)
    return true;

  size_t new_capacity = *capacity ? *capacity : 8u;

  while (new_capacity < required) {
    if (new_capacity > SIZE_MAX / 2u)
      return false;

    new_capacity *= 2u;
  }

  if (new_capacity > SIZE_MAX / element_size)
    return false;

  void *new_data = realloc(*data, new_capacity * element_size);

  if (!new_data)
    return false;

  *data = new_data;
  *capacity = new_capacity;

  return true;
}

char *BLB_ModelDuplicateString(const char *string) {
  if (!string)
    return NULL;

  size_t length = strlen(string);
  char *copy = malloc(length + 1u);

  if (!copy)
    return NULL;

  memcpy(copy, string, length + 1u);

  return copy;
}

uint64_t BLB_ModelHashPath(const char *path) {
  uint64_t hash = UINT64_C(1469598103934665603);

  for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
    hash ^= (uint64_t)*p;
    hash *= UINT64_C(1099511628211);
  }

  return hash ? hash : UINT64_C(1);
}

void BLB_ModelFreePolygon(BLB_Polygon3D *polygon) {
  if (!polygon)
    return;

  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->normals);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon);
}

bool BLB_ModelCacheReserve(size_t required) {
  if (model_cache_capacity >= required)
    return true;

  size_t capacity = model_cache_capacity ? model_cache_capacity : BLB_MODEL_CACHE_INITIAL_CAPACITY;

  while (capacity < required) {
    if (capacity > SIZE_MAX / 2u)
      return false;

    capacity *= 2u;
  }

  if (capacity > SIZE_MAX / sizeof(*model_cache))
    return false;

  BLB_ModelGeometry *new_cache = realloc(model_cache, capacity * sizeof(*model_cache));

  if (!new_cache)
    return false;

  model_cache = new_cache;
  model_cache_capacity = capacity;

  return true;
}

BLB_ModelGeometry *BLB_ModelCacheFind(const char *path) {
  if (!path)
    return NULL;

  for (size_t i = 0; i < model_cache_count; ++i) {
    if (strcmp(model_cache[i].path, path) == 0)
      return &model_cache[i];
  }

  return NULL;
}

BLB_ModelGeometry *BLB_ModelCacheAdd(const char *path, BLB_Polygon3D *polygon) {
  if (!path || !polygon)
    return NULL;

  if (!BLB_ModelCacheReserve(model_cache_count + 1u))
    return NULL;

  char *path_copy = BLB_ModelDuplicateString(path);

  if (!path_copy)
    return NULL;

  BLB_ModelGeometry *geometry = &model_cache[model_cache_count++];

  geometry->path = path_copy;
  geometry->polygon = polygon;
  geometry->geometry_id = BLB_ModelHashPath(path);
  geometry->refs = 1u;

  return geometry;
}

void BLB_ModelCacheRetain(BLB_ModelGeometry *geometry) {
  if (!geometry)
    return;

  ++geometry->refs;
}

void BLB_ModelCacheRelease(BLB_Object3D *object) {
  if (!object || !object->polygon)
    return;

  for (size_t i = 0; i < model_cache_count; ++i) {
    BLB_ModelGeometry *geometry = &model_cache[i];

    if (geometry->polygon != object->polygon)
      continue;

    if (geometry->refs > 0)
      --geometry->refs;

    if (geometry->refs != 0)
      return;

    BLB_ModelFreePolygon(geometry->polygon);
    free(geometry->path);

    size_t last = model_cache_count - 1u;

    if (i != last)
      model_cache[i] = model_cache[last];

    --model_cache_count;

    return;
  }
}
