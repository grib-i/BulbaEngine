#ifndef BLB_OBJECT_AGGREGATION_H
#define BLB_OBJECT_AGGREGATION_H

#include "bulba/core/math3v/polygon.h"
#include "bulba/core/objects3d/objects3d.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BLB_RenderObjects3DCluster {
  AABB_3D *bounds;

  Mesh *mesh;
  BLB_Polygon3D *polygon;

  Mesh **lods;
  float *lod_errors;
  size_t lod_count;

  char *cache_path;
  char **objects_cache_paths;

  BLB_Object3D **objects;
  uint64_t *object_transform_revisions;

  uint32_t object_count;

  bool dirty;
  bool rendered;
} BLB_RenderObjects3DCluster;

typedef struct BLB_InstanceCullBlock {
  BLB_Vec3 center;
  float radius;
} BLB_InstanceCullBlock;

typedef struct BLB_RenderInstances3DGroup {
  BLB_Object3D **objects;
  size_t object_count;
  size_t object_capacity;
  bool enabled;
  bool rendered;
  void *render_batch;
  size_t render_batch_capacity;
  void *transform_cache;
  size_t transform_cache_capacity;
  uint8_t *render_levels;
  size_t render_levels_capacity;
  size_t visible_instances;
  size_t draw_calls;
  BLB_Vec3 bounds_center;
  float bounds_radius;
  bool bounds_valid;
  BLB_InstanceCullBlock *cull_blocks;
  size_t cull_block_count;
  bool cull_blocks_dirty;
} BLB_RenderInstances3DGroup;

typedef struct BLB_ObjectAggregation3D {
  BLB_RenderObjects3DCluster **clusters;
  size_t cluster_count;

  BLB_RenderInstances3DGroup **instance_groups;
  size_t instance_group_count;

  bool build_complete;
  size_t source_count;
  uint64_t source_signature;

  float cluster_distance;

  int hsa_levels;
  float hsa_error;

  BLB_Object3D **map_objects;
  BLB_RenderObjects3DCluster **map_clusters;
  BLB_RenderInstances3DGroup **map_instances;
  size_t map_capacity;
} BLB_ObjectAggregation3D;

BLB_ObjectAggregation3D *BLB_ObjectAggregation3D_Create(BLB_Object3D **objects, size_t object_count, float cluster_distance, int hsa_levels,
                                                        float hsa_error);
BLB_ObjectAggregation3D *BLB_ObjectAggregation3D_CreateLightweight(BLB_Object3D **objects, size_t object_count, float cluster_distance,
                                                                   int hsa_levels, float hsa_error);

void BLB_ObjectAggregation3D_Destroy(BLB_ObjectAggregation3D *aggregation);

BLB_RenderObjects3DCluster *BLB_ObjectAggregation3D_FindCluster(BLB_ObjectAggregation3D *aggregation, BLB_Object3D *object);
BLB_RenderInstances3DGroup *BLB_ObjectAggregation3D_FindInstanceGroup(BLB_ObjectAggregation3D *aggregation, BLB_Object3D *object);
bool BLB_ObjectAggregation3D_ClusterTransformChanged(const BLB_RenderObjects3DCluster *cluster);
bool BLB_ObjectAggregation3D_MatchesSource(const BLB_ObjectAggregation3D *aggregation, BLB_Object3D **objects, size_t object_count);
bool BLB_ObjectAggregation3D_RebindSource(BLB_ObjectAggregation3D *aggregation, BLB_Object3D **objects, BLB_Object3D **snapshots,
                                          size_t object_count);

void BLB_ObjectAggregation3D_ResetDrawState(BLB_ObjectAggregation3D *aggregation);

Mesh *BLB_ObjectAggregation3D_GetLOD(BLB_RenderObjects3DCluster *cluster, size_t level);
bool BLB_ObjectAggregation3D_EnsureLOD(BLB_ObjectAggregation3D *aggregation, BLB_RenderObjects3DCluster *cluster, size_t level);

void BLB_ObjectAggregation3D_RebuildCluster(BLB_ObjectAggregation3D *aggregation, BLB_RenderObjects3DCluster *cluster);

#endif
