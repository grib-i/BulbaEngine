#ifndef BLB_VISIBILITY_CACHE_H
#define BLB_VISIBILITY_CACHE_H

#include "bulba/core/math3v/math3v.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { BLB_VISIBILITY_OBJECT_3D = 0, BLB_VISIBILITY_OBJECT_2D = 1 } BLB_VisibilityObjectKind;

typedef struct BLB_RenderVisibilityCache BLB_RenderVisibilityCache;

BLB_RenderVisibilityCache *BLB_RenderVisibilityCache_Create(void);
void BLB_RenderVisibilityCache_Destroy(BLB_RenderVisibilityCache *cache);

void BLB_RenderVisibilityCache_BeginFrame(BLB_RenderVisibilityCache *cache, BLB_Vec3 camera_position, const BLB_Mat4 *view_projection,
                                          float viewport_width, float viewport_height);

void BLB_RenderVisibilityCache_Reserve(BLB_RenderVisibilityCache *cache, size_t object_count);
void BLB_RenderVisibilityCache_Clear(BLB_RenderVisibilityCache *cache);

bool BLB_RenderVisibilityCache_Get(BLB_RenderVisibilityCache *cache, BLB_VisibilityObjectKind kind, const void *object, uint64_t transform_revision,
                                   bool *visible, float *distance);

void BLB_RenderVisibilityCache_Put(BLB_RenderVisibilityCache *cache, BLB_VisibilityObjectKind kind, const void *object, uint64_t transform_revision,
                                   bool visible, float distance);

bool BLB_RenderVisibilityCache_Flush(BLB_RenderVisibilityCache *cache);

void BLB_RenderVisibilityCache_GetStats(const BLB_RenderVisibilityCache *cache, uint64_t *hits, uint64_t *misses, size_t *entries);
uint64_t BLB_RenderVisibilityCache_GetCameraRevision(const BLB_RenderVisibilityCache *cache);

#endif
