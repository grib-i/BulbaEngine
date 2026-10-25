#ifndef BULBA_CORE_OBJECTS3D_MODEL_LOADER_COMMON_H
#define BULBA_CORE_OBJECTS3D_MODEL_LOADER_COMMON_H

#include "bulba/core/objects3d/objects3d.h"

#define BLB_MODEL_CACHE_INITIAL_CAPACITY 8u
#define BLB_MODEL_LINE_SIZE 16384u

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  char *path;
  BLB_Polygon3D *polygon;
  uint64_t geometry_id;
  size_t refs;
} BLB_ModelGeometry;

bool BLB_ModelArrayReserve(void **data, size_t *capacity, size_t required,
                           size_t element_size);
char *BLB_ModelDuplicateString(const char *string);
uint64_t BLB_ModelHashPath(const char *path);

void BLB_ModelFreePolygon(BLB_Polygon3D *polygon);
bool BLB_ModelCacheReserve(size_t required);

BLB_Vec3 BLB_ModelCalculateFaceNormal(BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c);
BLB_ModelGeometry *BLB_ModelCacheFind(const char *path);
BLB_ModelGeometry *BLB_ModelCacheAdd(const char *path, BLB_Polygon3D *polygon);

void BLB_ModelCacheRetain(BLB_ModelGeometry *geometry);
void BLB_ModelCacheRelease(BLB_Object3D *object);

extern BLB_ModelGeometry *model_cache;
extern size_t model_cache_count;
extern size_t model_cache_capacity;

#endif
