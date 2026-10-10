#include "bulba/core/render/object_aggregation.h"

#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/math3v/physics3d.h"
#include "bulba/core/render/mesh_simplification.h"
#include "bulba/core/utils/config.h"
#include <float.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int64_t x;
  int64_t y;
  int64_t z;
  uintptr_t material;
  uintptr_t texture;
  int layer;
  int render_mode;
  unsigned char color[4];
  bool used;
  BLB_RenderObjects3DCluster *cluster;
} BLB_ClusterHashEntry;

typedef struct {
  uintptr_t polygon;
  uint64_t geometry_id;
  uintptr_t material;
  uintptr_t texture;
  uintptr_t shader;
  int layer;
  int render_mode;
  unsigned char color[4];
  bool used;
  BLB_RenderInstances3DGroup *group;
} BLB_InstanceHashEntry;

static const BLB_Polygon3D *active_polygon(const BLB_Object3D *object) { return object ? object->polygon : NULL; }

static int object_render_mode(const BLB_Object3D *object) {
  if (!object)
    return BLB_RENDER_OPAQUE;

  if (object->material)
    return object->material->render_mode;

  return object->render_mode;
}

static uint64_t hash_pointer(uintptr_t value) {
  uint64_t x = (uint64_t)value;

  x ^= x >> 30;
  x *= UINT64_C(0xbf58476d1ce4e5b9);
  x ^= x >> 27;
  x *= UINT64_C(0x94d049bb133111eb);
  x ^= x >> 31;

  return x;
}

static uint64_t hash_cluster_key(int64_t x, int64_t y, int64_t z, uintptr_t material, uintptr_t texture, int layer, int render_mode, HMM_Vec4 color) {
  uint64_t hash = UINT64_C(1469598103934665603);

  hash ^= (uint64_t)x;
  hash *= UINT64_C(1099511628211);

  hash ^= (uint64_t)y;
  hash *= UINT64_C(1099511628211);

  hash ^= (uint64_t)z;
  hash *= UINT64_C(1099511628211);

  hash ^= hash_pointer(material);
  hash *= UINT64_C(1099511628211);

  hash ^= hash_pointer(texture);
  hash *= UINT64_C(1099511628211);

  hash ^= (uint64_t)(uint32_t)layer;
  hash *= UINT64_C(1099511628211);

  hash ^= (uint64_t)(uint32_t)render_mode;
  hash *= UINT64_C(1099511628211);

  const float values[4] = {color.x, color.y, color.z, color.w};
  for (size_t i = 0; i < 4; ++i) {
    uint32_t bits;
    memcpy(&bits, &values[i], sizeof(bits));
    hash ^= bits;
    hash *= UINT64_C(1099511628211);
  }

  return hash;
}

static uint64_t hash_instance_key(uintptr_t polygon, uint64_t geometry_id, uintptr_t material, uintptr_t texture, uintptr_t shader, int layer,
                                  int render_mode) {
  uint64_t hash = UINT64_C(1469598103934665603);
  const uintptr_t values[] = {polygon, material, texture, shader};
  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
    hash ^= hash_pointer(values[i]);
    hash *= UINT64_C(1099511628211);
  }
  hash ^= geometry_id;
  hash *= UINT64_C(1099511628211);
  hash ^= (uint64_t)(uint32_t)layer;
  hash *= UINT64_C(1099511628211);
  hash ^= (uint64_t)(uint32_t)render_mode;
  hash *= UINT64_C(1099511628211);
  return hash;
}

static uint64_t source_signature(BLB_Object3D **objects, size_t object_count) {
  uint64_t hash = UINT64_C(1469598103934665603);

  hash ^= BLB_OBJECT_AGGREGATION ? 1u : 0u;
  hash *= UINT64_C(1099511628211);

  hash ^= (uint64_t)(uint32_t)BLB_OBJECT_AGGREGATION_MIN_OBJECTS;
  hash *= UINT64_C(1099511628211);

  hash ^= BLB_OBJECT_INSTANCING ? 1u : 0u;
  hash *= UINT64_C(1099511628211);

  hash ^= (uint64_t)(uint32_t)BLB_OBJECT_INSTANCING_MIN_OBJECTS;
  hash *= UINT64_C(1099511628211);

  for (size_t i = 0; i < object_count; ++i) {
    BLB_Object3D *object = objects ? objects[i] : NULL;
    uintptr_t values[5] = {
        (uintptr_t)object,
        (uintptr_t)(object ? object->polygon : NULL),
        (uintptr_t)(object ? object->material : NULL),
        (uintptr_t)(object ? object->texture : NULL),
        (uintptr_t)(object && object->material ? object->material->shader_program : NULL),
    };

    for (size_t v = 0; v < 5; ++v) {
      hash ^= hash_pointer(values[v]);
      hash *= UINT64_C(1099511628211);
    }

    if (object) {
      hash ^= object->geometry_id;
      hash *= UINT64_C(1099511628211);

      hash ^= object->lod_override ? 1u : 0u;
      hash *= UINT64_C(1099511628211);

      hash ^= (uint64_t)(uint32_t)object->lod_level;
      hash *= UINT64_C(1099511628211);

      hash ^= object->material ? object->material->revision : 0u;
      hash *= UINT64_C(1099511628211);

      hash ^= (uint64_t)object->layer;
      hash *= UINT64_C(1099511628211);

      hash ^= (uint64_t)(uint32_t)object_render_mode(object);
      hash *= UINT64_C(1099511628211);

      for (size_t c = 0; c < 4; ++c) {
        uint32_t bits;
        memcpy(&bits, &object->color.Elements[c], sizeof(bits));
        hash ^= bits;
        hash *= UINT64_C(1099511628211);
      }
    }
  }
  return hash ^ (uint64_t)object_count;
}

static size_t next_capacity(size_t value) {
  size_t capacity = 16;

  while (capacity < value)
    capacity <<= 1;

  return capacity;
}

static char *create_cluster_cache_path(size_t index) {
  size_t size = strlen(BLB_DEFAULT_CACHE_PATH) + 64;
  char *path = malloc(size);

  if (!path)
    return NULL;

  snprintf(path, size, "%s/obj_agg/cluster_%zu", BLB_DEFAULT_CACHE_PATH, index);

  return path;
}

static char *create_object_cache_path(const char *cluster_path, const BLB_Object3D *object) {
  if (!cluster_path || !object || !object->id)
    return NULL;

  const char *type = object->id->id_type;

  if (!type || type[0] == '\0')
    type = "object";

  size_t size = strlen(cluster_path) + strlen(type) + 64;
  char *path = malloc(size);

  if (!path)
    return NULL;

  snprintf(path, size, "%s/%s_%llu.b3o", cluster_path, type, (unsigned long long)object->id->id);

  return path;
}

static void destroy_cluster_mesh(BLB_RenderObjects3DCluster *cluster) {
  if (!cluster)
    return;

  if (cluster->lods) {
    for (size_t i = 1; i < cluster->lod_count; i++)
      BLB_MeshDestroy(cluster->lods[i]);

    free(cluster->lods);
    cluster->lods = NULL;
  }

  free(cluster->lod_errors);
  cluster->lod_errors = NULL;
  cluster->lod_count = 0;

  free(cluster->polygon);
  cluster->polygon = NULL;

  BLB_MeshDestroy(cluster->mesh);
  cluster->mesh = NULL;
}

static void destroy_cluster(BLB_RenderObjects3DCluster *cluster) {
  if (!cluster)
    return;

  for (size_t i = 0; i < cluster->object_count; ++i) {
    if (cluster->objects[i] && cluster->objects[i]->optimization_cluster == cluster)
      cluster->objects[i]->optimization_cluster = NULL;
  }

  free(cluster->bounds);

  destroy_cluster_mesh(cluster);

  free(cluster->cache_path);

  if (cluster->objects_cache_paths) {
    for (size_t i = 0; i < cluster->object_count; i++)
      free(cluster->objects_cache_paths[i]);

    free(cluster->objects_cache_paths);
  }

  free(cluster->objects);
  free(cluster->object_transform_revisions);

  free(cluster);
}

static bool cluster_add_object(BLB_RenderObjects3DCluster *cluster, BLB_Object3D *object) {
  if (!cluster || !object)
    return false;

  BLB_Object3D **objects = realloc(cluster->objects, (cluster->object_count + 1) * sizeof(*objects));

  if (!objects)
    return false;

  cluster->objects = objects;

  char **cache_paths = realloc(cluster->objects_cache_paths, (cluster->object_count + 1) * sizeof(*cache_paths));

  if (!cache_paths)
    return false;

  cluster->objects_cache_paths = cache_paths;

  uint64_t *revisions = realloc(cluster->object_transform_revisions, (cluster->object_count + 1) * sizeof(*revisions));

  if (!revisions)
    return false;

  cluster->object_transform_revisions = revisions;
  cluster->objects[cluster->object_count] = object;
  cluster->objects_cache_paths[cluster->object_count] = create_object_cache_path(cluster->cache_path, object);
  cluster->object_transform_revisions[cluster->object_count] = object->transform_revision;
  object->optimization_cluster = cluster;
  cluster->object_count++;

  return true;
}

static bool append_cluster(BLB_ObjectAggregation3D *aggregation, BLB_RenderObjects3DCluster *cluster) {
  if (!aggregation || !cluster)
    return false;

  BLB_RenderObjects3DCluster **clusters = realloc(aggregation->clusters, (aggregation->cluster_count + 1) * sizeof(*clusters));

  if (!clusters)
    return false;

  aggregation->clusters = clusters;
  aggregation->clusters[aggregation->cluster_count] = cluster;
  aggregation->cluster_count++;

  return true;
}

static BLB_RenderObjects3DCluster *find_cluster_by_key(BLB_ClusterHashEntry *table, size_t capacity, int64_t x, int64_t y, int64_t z,
                                                       uintptr_t material, uintptr_t texture, int layer, int render_mode, HMM_Vec4 color) {
  uint64_t hash = hash_cluster_key(x, y, z, material, texture, layer, render_mode, color);

  size_t index = (size_t)(hash & (capacity - 1));

  while (table[index].used) {
    if (table[index].x == x && table[index].y == y && table[index].z == z && table[index].material == material && table[index].texture == texture &&
        table[index].layer == layer && table[index].render_mode == render_mode && memcmp(table[index].color, &color, 4) == 0)
      return table[index].cluster;

    index = (index + 1) & (capacity - 1);
  }

  return NULL;
}

static bool insert_cluster_key(BLB_ClusterHashEntry *table, size_t capacity, int64_t x, int64_t y, int64_t z, uintptr_t material, uintptr_t texture,
                               int layer, int render_mode, HMM_Vec4 color, BLB_RenderObjects3DCluster *cluster) {
  uint64_t hash = hash_cluster_key(x, y, z, material, texture, layer, render_mode, color);

  size_t index = (size_t)(hash & (capacity - 1));

  while (table[index].used)
    index = (index + 1) & (capacity - 1);

  table[index].used = true;
  table[index].x = x;
  table[index].y = y;
  table[index].z = z;
  table[index].material = material;
  table[index].texture = texture;
  table[index].layer = layer;
  table[index].render_mode = render_mode;
  memcpy(table[index].color, &color, 4);
  table[index].cluster = cluster;

  return true;
}

static bool object_has_dynamic_physics(const BLB_Object3D *object) {
  if (!object || !object->rigid_body)
    return false;
  BLB_Physics3DBodyType type = BLB_Physics3D_BodyGetType((const BLB_RigidBody3D *)object->rigid_body);
  return type == BLB_PHYSICS3D_DYNAMIC || type == BLB_PHYSICS3D_KINEMATIC;
}

static bool object_can_aggregate(const BLB_Object3D *object) {
  if (!object || object->submesh_count > 0 || !active_polygon(object) || !object->material)
    return false;

  if (object_render_mode(object) != BLB_RENDER_OPAQUE)
    return false;

  if (object->material->shader_program)
    return false;

  if (fabsf(object->material->emission_strength) > 0.00001f || fabsf(object->material->glow_strength) > 0.00001f)
    return false;

  if (BLB_OBJECT_LOD && object->lod_override)
    return false;

  if (BLB_OBJECT_AGGREGATION_STATIC_ONLY &&
      (object_has_dynamic_physics(object) || object->optimization_dynamic || object->optimization_transform_dirty))
    return false;

  const BLB_Polygon3D *polygon = active_polygon(object);
  return polygon->vertex_count > 0 && polygon->indices && polygon->index_count >= 3;
}

static bool object_can_instance(const BLB_Object3D *object) {
  if (!object || object->submesh_count > 0 || !active_polygon(object) || !object->material)
    return false;

  if (object_render_mode(object) != BLB_RENDER_OPAQUE)
    return false;

  if (object->material->shader_program)
    return false;

  if (fabsf(object->material->emission_strength) > 0.00001f || fabsf(object->material->glow_strength) > 0.00001f)
    return false;

  const BLB_Polygon3D *polygon = active_polygon(object);
  return polygon->vertex_count > 0 && polygon->indices && polygon->index_count >= 3;
}

static bool instance_key_equal(const BLB_InstanceHashEntry *entry, const BLB_Object3D *object) {
  if (!entry || !object)
    return false;

  uintptr_t shader = object->material ? (uintptr_t)object->material->shader_program : 0;
  return entry->polygon == (uintptr_t)object->polygon && entry->geometry_id == object->geometry_id &&
         entry->material == (uintptr_t)object->material && entry->texture == (uintptr_t)object->texture && entry->shader == shader &&
         entry->layer == object->layer && entry->render_mode == object_render_mode(object);
}

static BLB_RenderInstances3DGroup *find_instance_group_by_key(BLB_InstanceHashEntry *table, size_t capacity, const BLB_Object3D *object) {
  if (!table || !capacity || !object)
    return NULL;

  uintptr_t shader = object->material ? (uintptr_t)object->material->shader_program : 0;
  uint64_t hash = hash_instance_key((uintptr_t)object->polygon, object->geometry_id, (uintptr_t)object->material, (uintptr_t)object->texture, shader,
                                    object->layer, object_render_mode(object));
  size_t index = (size_t)(hash & (capacity - 1));

  while (table[index].used) {
    if (instance_key_equal(&table[index], object))
      return table[index].group;

    index = (index + 1) & (capacity - 1);
  }

  return NULL;
}

static bool insert_instance_group_key(BLB_InstanceHashEntry *table, size_t capacity, const BLB_Object3D *object, BLB_RenderInstances3DGroup *group) {
  if (!table || !capacity || !object || !group)
    return false;

  uintptr_t shader = object->material ? (uintptr_t)object->material->shader_program : 0;
  uint64_t hash = hash_instance_key((uintptr_t)object->polygon, object->geometry_id, (uintptr_t)object->material, (uintptr_t)object->texture, shader,
                                    object->layer, object_render_mode(object));
  size_t index = (size_t)(hash & (capacity - 1));

  while (table[index].used)
    index = (index + 1) & (capacity - 1);
  table[index].used = true;
  table[index].polygon = (uintptr_t)object->polygon;
  table[index].geometry_id = object->geometry_id;
  table[index].material = (uintptr_t)object->material;
  table[index].texture = (uintptr_t)object->texture;
  table[index].shader = shader;
  table[index].layer = object->layer;
  table[index].render_mode = object_render_mode(object);

  memcpy(table[index].color, &object->color, sizeof(table[index].color));
  table[index].group = group;

  return true;
}

static void update_instance_group_bounds(BLB_RenderInstances3DGroup *group) {
  if (!group)
    return;

  if (group->object_count == 0) {
    group->bounds_center = HMM_V3(0.0f, 0.0f, 0.0f);
    group->bounds_radius = 0.0f;
    group->bounds_valid = false;
    return;
  }

  HMM_Vec3 min_bounds = HMM_V3(FLT_MAX, FLT_MAX, FLT_MAX);
  HMM_Vec3 max_bounds = HMM_V3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  bool valid = true;

  for (size_t i = 0; i < group->object_count; ++i) {
    BLB_Object3D *object = group->objects[i];
    if (!object || object->optimization_dynamic || object->optimization_transform_dirty || object_has_dynamic_physics(object)) {
      valid = false;
      break;
    }

    const BLB_Polygon3D *polygon = active_polygon(object);
    if (!polygon || !polygon->vertices || polygon->vertex_count == 0) {
      valid = false;
      break;
    }

    float local_radius = 0.0f;
    for (size_t v = 0; v < polygon->vertex_count; ++v) {
      float value = HMM_LenV3(polygon->vertices[v]);
      if (value > local_radius)
        local_radius = value;
    }

    float scale = fmaxf(fabsf(object->scale.x), fmaxf(fabsf(object->scale.y), fabsf(object->scale.z)));
    float radius = local_radius * scale;

    HMM_Vec3 min = HMM_SubV3(object->position, HMM_V3(radius, radius, radius));
    HMM_Vec3 max = HMM_AddV3(object->position, HMM_V3(radius, radius, radius));

    min_bounds.x = fminf(min_bounds.x, min.x);
    min_bounds.y = fminf(min_bounds.y, min.y);
    min_bounds.z = fminf(min_bounds.z, min.z);

    max_bounds.x = fmaxf(max_bounds.x, max.x);
    max_bounds.y = fmaxf(max_bounds.y, max.y);
    max_bounds.z = fmaxf(max_bounds.z, max.z);
  }

  if (!valid) {
    group->bounds_valid = false;
    return;
  }

  group->bounds_center = HMM_MulV3F(HMM_AddV3(min_bounds, max_bounds), 0.5f);
  group->bounds_radius = HMM_LenV3(HMM_MulV3F(HMM_SubV3(max_bounds, min_bounds), 0.5f));
  group->bounds_valid = group->bounds_radius > 0.0f;
}

static bool instance_group_add_object(BLB_RenderInstances3DGroup *group, BLB_Object3D *object) {
  if (!group || !object)
    return false;

  if (group->object_count >= group->object_capacity) {
    size_t capacity = group->object_capacity ? group->object_capacity * 2 : 16;
    BLB_Object3D **objects = realloc(group->objects, capacity * sizeof(*objects));

    if (!objects)
      return false;

    group->objects = objects;
    group->object_capacity = capacity;
  }

  group->objects[group->object_count++] = object;
  object->optimization_instance_group = group;
  return true;
}

static bool append_instance_group(BLB_ObjectAggregation3D *aggregation, BLB_RenderInstances3DGroup *group) {
  if (!aggregation || !group)
    return false;

  BLB_RenderInstances3DGroup **groups = realloc(aggregation->instance_groups, (aggregation->instance_group_count + 1) * sizeof(*groups));

  if (!groups)
    return false;

  aggregation->instance_groups = groups;
  aggregation->instance_groups[aggregation->instance_group_count++] = group;
  return true;
}

static void destroy_instance_group(BLB_RenderInstances3DGroup *group) {
  if (!group)
    return;

  for (size_t i = 0; i < group->object_count; ++i) {
    if (group->objects[i] && group->objects[i]->optimization_instance_group == group)
      group->objects[i]->optimization_instance_group = NULL;
  }

  free(group->objects);
  free(group);
}

static bool create_cluster_polygon(BLB_RenderObjects3DCluster *cluster) {
  if (!cluster || !cluster->mesh)
    return false;

  BLB_Polygon3D *polygon = calloc(1, sizeof(*polygon));

  if (!polygon)
    return false;

  polygon->vertices = cluster->mesh->vertices;
  polygon->normals = cluster->mesh->normals;
  polygon->uvs = cluster->mesh->uvs;
  polygon->vertex_count = cluster->mesh->vertex_count;
  polygon->indices = cluster->mesh->indices;
  polygon->index_count = cluster->mesh->index_count;
  polygon->normal = HMM_V3(0.0f, 1.0f, 0.0f);

  cluster->polygon = polygon;

  return true;
}

static bool create_cluster_mesh(BLB_RenderObjects3DCluster *cluster) {
  if (!cluster || cluster->object_count < 2)
    return false;

  size_t vertex_count = 0;
  size_t index_count = 0;
  bool has_uvs = true;

  for (size_t i = 0; i < cluster->object_count; i++) {
    BLB_Object3D *object = cluster->objects[i];

    if (!object || !object->polygon)
      return false;

    BLB_Polygon3D *polygon = (BLB_Polygon3D *)object->polygon;

    vertex_count += polygon->vertex_count;

    if (polygon->indices && polygon->index_count > 0)
      index_count += polygon->index_count;
    else
      index_count += polygon->vertex_count;

    if (!polygon->uvs)
      has_uvs = false;
  }

  if (vertex_count == 0 || index_count == 0)
    return false;

  Mesh *mesh = calloc(1, sizeof(*mesh));

  if (!mesh)
    return false;

  mesh->vertices = malloc(vertex_count * sizeof(*mesh->vertices));
  mesh->normals = calloc(vertex_count, sizeof(*mesh->normals));
  mesh->indices = malloc(index_count * sizeof(*mesh->indices));

  if (has_uvs)
    mesh->uvs = malloc(vertex_count * sizeof(*mesh->uvs));

  if (!mesh->vertices || !mesh->normals || !mesh->indices || (has_uvs && !mesh->uvs)) {
    BLB_MeshDestroy(mesh);
    return false;
  }

  mesh->vertex_count = vertex_count;
  mesh->index_count = index_count;

  size_t vertex_offset = 0;
  size_t index_offset = 0;

  HMM_Vec3 bounds_min = HMM_V3(INFINITY, INFINITY, INFINITY);
  HMM_Vec3 bounds_max = HMM_V3(-INFINITY, -INFINITY, -INFINITY);

  for (size_t i = 0; i < cluster->object_count; i++) {
    BLB_Object3D *object = cluster->objects[i];
    BLB_Polygon3D *polygon = (BLB_Polygon3D *)object->polygon;

    HMM_Mat4 rx = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.x), HMM_V3(1.0f, 0.0f, 0.0f));
    HMM_Mat4 ry = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.y), HMM_V3(0.0f, 1.0f, 0.0f));
    HMM_Mat4 rz = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.z), HMM_V3(0.0f, 0.0f, 1.0f));

    HMM_Mat4 rotation = HMM_MulM4(rz, HMM_MulM4(ry, rx));
    HMM_Mat4 model = HMM_MulM4(HMM_Translate(object->position), HMM_MulM4(rotation, HMM_Scale(object->scale)));

    for (size_t v = 0; v < polygon->vertex_count; v++) {
      HMM_Vec4 world = HMM_MulM4V4(model, HMM_V4(polygon->vertices[v].x, polygon->vertices[v].y, polygon->vertices[v].z, 1.0f));

      HMM_Vec3 position = HMM_V3(world.x, world.y, world.z);

      mesh->vertices[vertex_offset + v] = position;

      if (position.x < bounds_min.x)
        bounds_min.x = position.x;

      if (position.y < bounds_min.y)
        bounds_min.y = position.y;

      if (position.z < bounds_min.z)
        bounds_min.z = position.z;

      if (position.x > bounds_max.x)
        bounds_max.x = position.x;

      if (position.y > bounds_max.y)
        bounds_max.y = position.y;

      if (position.z > bounds_max.z)
        bounds_max.z = position.z;

      if (has_uvs && polygon->uvs)
        mesh->uvs[vertex_offset + v] = polygon->uvs[v];

      if (polygon->normals) {
        HMM_Vec3 normal = polygon->normals[v];
        float sx = fabsf(object->scale.x) > 0.000001f ? object->scale.x : (object->scale.x < 0.0f ? -0.000001f : 0.000001f);
        float sy = fabsf(object->scale.y) > 0.000001f ? object->scale.y : (object->scale.y < 0.0f ? -0.000001f : 0.000001f);
        float sz = fabsf(object->scale.z) > 0.000001f ? object->scale.z : (object->scale.z < 0.0f ? -0.000001f : 0.000001f);
        normal.x /= sx;
        normal.y /= sy;
        normal.z /= sz;
        HMM_Vec4 transformed_normal = HMM_MulM4V4(rotation, HMM_V4(normal.x, normal.y, normal.z, 0.0f));
        HMM_Vec3 world_normal = HMM_V3(transformed_normal.x, transformed_normal.y, transformed_normal.z);
        float normal_length = HMM_LenV3(world_normal);
        if (normal_length > 0.000001f)
          world_normal = HMM_MulV3F(world_normal, 1.0f / normal_length);
        mesh->normals[vertex_offset + v] = world_normal;
      }
    }

    if (polygon->indices && polygon->index_count > 0) {
      for (size_t j = 0; j < polygon->index_count; j++)
        mesh->indices[index_offset + j] = (unsigned int)(vertex_offset + polygon->indices[j]);

      index_offset += polygon->index_count;
    } else {
      for (size_t j = 0; j < polygon->vertex_count; j++)
        mesh->indices[index_offset + j] = (unsigned int)(vertex_offset + j);

      index_offset += polygon->vertex_count;
    }

    vertex_offset += polygon->vertex_count;
  }

  for (size_t i = 0; i + 2 < mesh->index_count; i += 3) {
    unsigned int ia = mesh->indices[i];
    unsigned int ib = mesh->indices[i + 1];
    unsigned int ic = mesh->indices[i + 2];
    if (ia >= mesh->vertex_count || ib >= mesh->vertex_count || ic >= mesh->vertex_count)
      continue;

    HMM_Vec3 a = mesh->vertices[ia];
    HMM_Vec3 b = mesh->vertices[ib];
    HMM_Vec3 c = mesh->vertices[ic];
    HMM_Vec3 normal = HMM_Cross(HMM_SubV3(b, a), HMM_SubV3(c, a));
    if (HMM_LenV3(normal) <= 0.000001f)
      continue;

    if (HMM_LenV3(mesh->normals[ia]) <= 0.000001f)
      mesh->normals[ia] = HMM_AddV3(mesh->normals[ia], normal);
    if (HMM_LenV3(mesh->normals[ib]) <= 0.000001f)
      mesh->normals[ib] = HMM_AddV3(mesh->normals[ib], normal);
    if (HMM_LenV3(mesh->normals[ic]) <= 0.000001f)
      mesh->normals[ic] = HMM_AddV3(mesh->normals[ic], normal);
  }

  for (size_t i = 0; i < mesh->vertex_count; i++) {
    float length = HMM_LenV3(mesh->normals[i]);
    if (length > 0.000001f)
      mesh->normals[i] = HMM_MulV3F(mesh->normals[i], 1.0f / length);
    else
      mesh->normals[i] = HMM_V3(0.0f, 1.0f, 0.0f);
  }

  AABB_3D *bounds = calloc(1, sizeof(*bounds));

  if (bounds) {
    bounds->min = bounds_min;
    bounds->max = bounds_max;
  }

  cluster->bounds = bounds;
  cluster->mesh = mesh;

  if (!create_cluster_polygon(cluster)) {
    BLB_MeshDestroy(cluster->mesh);
    cluster->mesh = NULL;
    free(cluster->bounds);
    cluster->bounds = NULL;
    return false;
  }

  cluster->dirty = false;

  return true;
}

static float mesh_diagonal(const Mesh *mesh) {
  if (!mesh || !mesh->vertices || mesh->vertex_count == 0)
    return 0.0f;

  HMM_Vec3 min_v = mesh->vertices[0];
  HMM_Vec3 max_v = mesh->vertices[0];

  for (size_t i = 1; i < mesh->vertex_count; i++) {
    HMM_Vec3 v = mesh->vertices[i];

    if (v.x < min_v.x)
      min_v.x = v.x;

    if (v.y < min_v.y)
      min_v.y = v.y;

    if (v.z < min_v.z)
      min_v.z = v.z;

    if (v.x > max_v.x)
      max_v.x = v.x;

    if (v.y > max_v.y)
      max_v.y = v.y;

    if (v.z > max_v.z)
      max_v.z = v.z;
  }

  return HMM_LenV3(HMM_SubV3(max_v, min_v));
}

static bool build_hsa_lods(BLB_RenderObjects3DCluster *cluster) {
  if (!cluster || !cluster->mesh)
    return false;

  size_t levels = BLB_OBJECT_LOD && BLB_OBJECT_LOD_LEVELS > 0 ? (size_t)BLB_OBJECT_LOD_LEVELS + 1u : 1u;
  Mesh **lods = calloc(levels, sizeof(*lods));
  float *lod_errors = calloc(levels, sizeof(*lod_errors));

  if (!lods || !lod_errors) {
    free(lods);
    free(lod_errors);
    return false;
  }

  lods[0] = cluster->mesh;
  cluster->lods = lods;
  cluster->lod_errors = lod_errors;
  cluster->lod_count = 1;
  return true;
}

bool BLB_ObjectAggregation3D_EnsureLOD(BLB_ObjectAggregation3D *aggregation, BLB_RenderObjects3DCluster *cluster, size_t level) {
  (void)aggregation;

  if (!cluster || level == 0 || !BLB_OBJECT_LOD || !cluster->lods || level >= (size_t)(BLB_OBJECT_LOD_LEVELS + 1))
    return level == 0 && cluster != NULL;

  if (cluster->lod_count > level && cluster->lods[level])
    return true;

  size_t target = cluster->lod_count;
  if (target == 0)
    target = 1;

  Mesh *current = cluster->lods[target - 1];
  while (target <= level) {
    if (!current)
      return false;

    float error = BLB_OBJECT_LOD_ERROR * powf(2.0f, (float)(target - 1));
    if (error <= 0.0f)
      error = 0.000001f;

    Mesh *next = BLB_MeshSimplifyQEMWithReduction(current, error, BLB_OBJECT_LOD_REDUCTION);
    if ((!next || next->index_count >= current->index_count)) {
      BLB_MeshDestroy(next);
      float fallback = fmaxf(BLB_OBJECT_LOD_FALLBACK_ERROR, error);
      next = BLB_MeshSimplifyQEMWithReduction(current, fallback, BLB_OBJECT_LOD_REDUCTION);
    }

    if (!next || next->index_count >= current->index_count) {
      BLB_MeshDestroy(next);
      break;
    }

    cluster->lods[target] = next;
    cluster->lod_errors[target] = mesh_diagonal(next);
    cluster->lod_count = target + 1u;
    current = next;
    ++target;
  }

  return cluster->lod_count > level && cluster->lods[level] != NULL;
}

static bool build_object_map(BLB_ObjectAggregation3D *aggregation) {
  if (!aggregation)
    return false;

  size_t total_objects = 0;
  for (size_t i = 0; i < aggregation->cluster_count; i++) {
    BLB_RenderObjects3DCluster *cluster = aggregation->clusters[i];
    if (cluster)
      total_objects += cluster->object_count;
  }

  for (size_t i = 0; i < aggregation->instance_group_count; i++) {
    BLB_RenderInstances3DGroup *group = aggregation->instance_groups[i];
    if (group && group->enabled)
      total_objects += group->object_count;
  }

  aggregation->map_capacity = next_capacity(total_objects * 2 + 1);
  aggregation->map_objects = calloc(aggregation->map_capacity, sizeof(*aggregation->map_objects));
  aggregation->map_clusters = calloc(aggregation->map_capacity, sizeof(*aggregation->map_clusters));
  aggregation->map_instances = calloc(aggregation->map_capacity, sizeof(*aggregation->map_instances));

  if (!aggregation->map_objects || !aggregation->map_clusters || !aggregation->map_instances)
    return false;

  for (size_t i = 0; i < aggregation->cluster_count; i++) {
    BLB_RenderObjects3DCluster *cluster = aggregation->clusters[i];

    if (!cluster || cluster->object_count < 2)
      continue;

    for (size_t j = 0; j < cluster->object_count; j++) {
      BLB_Object3D *object = cluster->objects[j];
      size_t index = (size_t)(hash_pointer((uintptr_t)object) & (aggregation->map_capacity - 1));
      while (aggregation->map_objects[index])
        index = (index + 1) & (aggregation->map_capacity - 1);
      aggregation->map_objects[index] = object;
      aggregation->map_clusters[index] = cluster;
    }
  }

  for (size_t i = 0; i < aggregation->instance_group_count; i++) {
    BLB_RenderInstances3DGroup *group = aggregation->instance_groups[i];
    if (!group || !group->enabled)
      continue;
    for (size_t j = 0; j < group->object_count; j++) {
      BLB_Object3D *object = group->objects[j];
      size_t index = (size_t)(hash_pointer((uintptr_t)object) & (aggregation->map_capacity - 1));
      while (aggregation->map_objects[index])
        index = (index + 1) & (aggregation->map_capacity - 1);
      aggregation->map_objects[index] = object;
      aggregation->map_instances[index] = group;
    }
  }

  return true;
}

static bool build_cluster(BLB_ObjectAggregation3D *aggregation, BLB_RenderObjects3DCluster *cluster) {
  if (!aggregation || !cluster)
    return false;

  destroy_cluster_mesh(cluster);

  if (cluster->object_count < 2) {
    cluster->dirty = false;
    return true;
  }

  if (!create_cluster_mesh(cluster)) {
    cluster->dirty = true;
    return false;
  }

  if (!build_hsa_lods(cluster)) {
    cluster->dirty = true;
    return false;
  }

  for (size_t i = 0; i < cluster->object_count; ++i) {
    if (cluster->objects[i])
      cluster->object_transform_revisions[i] = cluster->objects[i]->transform_revision;
  }

  cluster->dirty = false;

  return true;
}

static BLB_ObjectAggregation3D *create_aggregation(BLB_Object3D **objects, size_t object_count, float cluster_distance, int hsa_levels,
                                                   float hsa_error, bool build_meshes) {
  if (!objects || object_count == 0)
    return NULL;

  if (cluster_distance <= 0.0f)
    cluster_distance = 1.0f;

  BLB_ObjectAggregation3D *aggregation = calloc(1, sizeof(*aggregation));
  if (!aggregation)
    return NULL;

  aggregation->source_count = object_count;
  aggregation->source_signature = source_signature(objects, object_count);
  aggregation->cluster_distance = cluster_distance;
  aggregation->hsa_levels = hsa_levels;
  aggregation->hsa_error = hsa_error;

  size_t hash_capacity = next_capacity(object_count * 2 + 1);
  BLB_ClusterHashEntry *cluster_table = calloc(hash_capacity, sizeof(*cluster_table));
  BLB_InstanceHashEntry *instance_table = calloc(hash_capacity, sizeof(*instance_table));
  if (!cluster_table || !instance_table) {
    free(cluster_table);
    free(instance_table);
    free(aggregation);
    return NULL;
  }

  if (BLB_OBJECT_INSTANCING) {
    for (size_t i = 0; i < object_count; i++) {
      BLB_Object3D *object = objects[i];
      if (!object_can_instance(object))
        continue;

      BLB_RenderInstances3DGroup *group = find_instance_group_by_key(instance_table, hash_capacity, object);
      if (!group) {
        group = calloc(1, sizeof(*group));
        if (!group || !append_instance_group(aggregation, group)) {
          destroy_instance_group(group);
          continue;
        }
        if (!insert_instance_group_key(instance_table, hash_capacity, object, group)) {
          aggregation->instance_group_count--;
          aggregation->instance_groups[aggregation->instance_group_count] = NULL;
          destroy_instance_group(group);
          continue;
        }
      }
      if (!instance_group_add_object(group, object)) {
        free(cluster_table);
        free(instance_table);
        BLB_ObjectAggregation3D_Destroy(aggregation);
        return NULL;
      }
    }

    for (size_t i = 0; i < aggregation->instance_group_count; i++) {
      BLB_RenderInstances3DGroup *group = aggregation->instance_groups[i];
      if (group) {
        group->enabled = group->object_count >= (size_t)(BLB_OBJECT_INSTANCING_MIN_OBJECTS > 1 ? BLB_OBJECT_INSTANCING_MIN_OBJECTS : 1);
        update_instance_group_bounds(group);
      }
    }
  }

  if (BLB_OBJECT_AGGREGATION) {
    for (size_t i = 0; i < object_count; i++) {
      BLB_Object3D *object = objects[i];
      BLB_RenderInstances3DGroup *instance_group = find_instance_group_by_key(instance_table, hash_capacity, object);
      if (instance_group && instance_group->enabled)
        continue;
      if (!object_can_aggregate(object))
        continue;

      int64_t x = (int64_t)floorf(object->position.x / cluster_distance);
      int64_t y = (int64_t)floorf(object->position.y / cluster_distance);
      int64_t z = (int64_t)floorf(object->position.z / cluster_distance);
      uintptr_t material = (uintptr_t)object->material;
      uintptr_t texture = (uintptr_t)object->texture;
      int layer = object->layer;
      int render_mode = object_render_mode(object);

      BLB_RenderObjects3DCluster *cluster =
          find_cluster_by_key(cluster_table, hash_capacity, x, y, z, material, texture, layer, render_mode, object->color);
      if (!cluster) {
        cluster = calloc(1, sizeof(*cluster));
        if (!cluster)
          continue;
        cluster->cache_path = create_cluster_cache_path(aggregation->cluster_count);
        cluster->dirty = true;
        if (!cluster->cache_path || !append_cluster(aggregation, cluster)) {
          destroy_cluster(cluster);
          continue;
        }
        insert_cluster_key(cluster_table, hash_capacity, x, y, z, material, texture, layer, render_mode, object->color, cluster);
      }
      if (!cluster_add_object(cluster, object)) {
        free(cluster_table);
        free(instance_table);
        BLB_ObjectAggregation3D_Destroy(aggregation);
        return NULL;
      }
    }
  }

  free(cluster_table);
  free(instance_table);

  for (size_t i = 0; i < aggregation->cluster_count;) {
    BLB_RenderObjects3DCluster *cluster = aggregation->clusters[i];
    if (!cluster || cluster->object_count < 2) {
      destroy_cluster(cluster);
      for (size_t j = i + 1; j < aggregation->cluster_count; j++)
        aggregation->clusters[j - 1] = aggregation->clusters[j];
      aggregation->cluster_count--;
      continue;
    }
    if (build_meshes) {
      if (!build_cluster(aggregation, cluster))
        cluster->dirty = true;
    } else {
      cluster->dirty = true;
    }
    i++;
  }

  if (!build_object_map(aggregation)) {
    BLB_ObjectAggregation3D_Destroy(aggregation);
    return NULL;
  }

  return aggregation;
}

BLB_ObjectAggregation3D *BLB_ObjectAggregation3D_Create(BLB_Object3D **objects, size_t object_count, float cluster_distance, int hsa_levels,
                                                        float hsa_error) {
  return create_aggregation(objects, object_count, cluster_distance, hsa_levels, hsa_error, true);
}

BLB_ObjectAggregation3D *BLB_ObjectAggregation3D_CreateLightweight(BLB_Object3D **objects, size_t object_count, float cluster_distance,
                                                                   int hsa_levels, float hsa_error) {
  return create_aggregation(objects, object_count, cluster_distance, hsa_levels, hsa_error, false);
}

void BLB_ObjectAggregation3D_Destroy(BLB_ObjectAggregation3D *aggregation) {
  if (!aggregation)
    return;

  for (size_t i = 0; i < aggregation->cluster_count; i++)
    destroy_cluster(aggregation->clusters[i]);

  free(aggregation->clusters);
  for (size_t i = 0; i < aggregation->instance_group_count; i++)
    destroy_instance_group(aggregation->instance_groups[i]);
  free(aggregation->instance_groups);
  free(aggregation->map_objects);
  free(aggregation->map_clusters);
  free(aggregation->map_instances);

  free(aggregation);
}

BLB_RenderObjects3DCluster *BLB_ObjectAggregation3D_FindCluster(BLB_ObjectAggregation3D *aggregation, BLB_Object3D *object) {
  (void)aggregation;
  return object ? object->optimization_cluster : NULL;
}

BLB_RenderInstances3DGroup *BLB_ObjectAggregation3D_FindInstanceGroup(BLB_ObjectAggregation3D *aggregation, BLB_Object3D *object) {
  (void)aggregation;
  return object ? object->optimization_instance_group : NULL;
}

bool BLB_ObjectAggregation3D_RebindSource(BLB_ObjectAggregation3D *aggregation, BLB_Object3D **objects, BLB_Object3D **snapshots,
                                          size_t object_count) {
  if (!aggregation || !objects || !snapshots || object_count != aggregation->source_count)
    return false;

  size_t capacity = 16;
  while (capacity < object_count * 2u + 1u)
    capacity <<= 1u;

  BLB_Object3D **map_snapshots = calloc(capacity, sizeof(*map_snapshots));
  BLB_Object3D **map_objects = calloc(capacity, sizeof(*map_objects));
  if (!map_snapshots || !map_objects) {
    free(map_snapshots);
    free(map_objects);
    return false;
  }

  for (size_t i = 0; i < object_count; ++i) {
    if (!snapshots[i] || !objects[i])
      continue;
    size_t index = (size_t)(hash_pointer((uintptr_t)snapshots[i]) & (capacity - 1u));
    while (map_snapshots[index])
      index = (index + 1u) & (capacity - 1u);
    map_snapshots[index] = snapshots[i];
    map_objects[index] = objects[i];
  }

  for (size_t i = 0; i < aggregation->instance_group_count; ++i) {
    BLB_RenderInstances3DGroup *group = aggregation->instance_groups[i];
    if (!group)
      continue;
    for (size_t j = 0; j < group->object_count; ++j) {
      BLB_Object3D *snapshot = group->objects[j];
      if (!snapshot)
        continue;
      size_t index = (size_t)(hash_pointer((uintptr_t)snapshot) & (capacity - 1u));
      while (map_snapshots[index] && map_snapshots[index] != snapshot)
        index = (index + 1u) & (capacity - 1u);
      if (map_snapshots[index] == snapshot) {
        group->objects[j] = map_objects[index];
        map_objects[index]->optimization_instance_group = group;
      }
    }
  }

  for (size_t i = 0; i < aggregation->cluster_count; ++i) {
    BLB_RenderObjects3DCluster *cluster = aggregation->clusters[i];
    if (!cluster)
      continue;
    for (size_t j = 0; j < cluster->object_count; ++j) {
      BLB_Object3D *snapshot = cluster->objects[j];
      if (!snapshot)
        continue;
      size_t index = (size_t)(hash_pointer((uintptr_t)snapshot) & (capacity - 1u));
      while (map_snapshots[index] && map_snapshots[index] != snapshot)
        index = (index + 1u) & (capacity - 1u);
      if (map_snapshots[index] == snapshot) {
        cluster->objects[j] = map_objects[index];
        map_objects[index]->optimization_cluster = cluster;
      }
    }
  }

  free(aggregation->map_objects);
  free(aggregation->map_clusters);
  free(aggregation->map_instances);
  aggregation->map_objects = NULL;
  aggregation->map_clusters = NULL;
  aggregation->map_instances = NULL;
  aggregation->map_capacity = 0;

  aggregation->source_signature = source_signature(objects, object_count);

  bool result = build_object_map(aggregation);
  free(map_snapshots);
  free(map_objects);
  return result;
}

bool BLB_ObjectAggregation3D_ClusterTransformChanged(const BLB_RenderObjects3DCluster *cluster) {
  if (!cluster || !cluster->objects || !cluster->object_transform_revisions)
    return false;
  for (size_t i = 0; i < cluster->object_count; i++) {
    if (cluster->objects[i] && cluster->objects[i]->transform_revision != cluster->object_transform_revisions[i])
      return true;
  }
  return false;
}

bool BLB_ObjectAggregation3D_MatchesSource(const BLB_ObjectAggregation3D *aggregation, BLB_Object3D **objects, size_t object_count) {
  if (!aggregation || !objects || aggregation->source_count != object_count)
    return false;
  return aggregation->source_signature == source_signature(objects, object_count);
}

void BLB_ObjectAggregation3D_ResetDrawState(BLB_ObjectAggregation3D *aggregation) {
  if (!aggregation)
    return;

  for (size_t i = 0; i < aggregation->cluster_count; i++) {
    if (aggregation->clusters[i])
      aggregation->clusters[i]->rendered = false;
  }
  for (size_t i = 0; i < aggregation->instance_group_count; i++) {
    if (aggregation->instance_groups[i])
      aggregation->instance_groups[i]->rendered = false;
  }
}

Mesh *BLB_ObjectAggregation3D_GetLOD(BLB_RenderObjects3DCluster *cluster, size_t level) {
  if (!cluster)
    return NULL;

  if (!cluster->lods || cluster->lod_count == 0)
    return cluster->mesh;

  if (level >= cluster->lod_count)
    level = cluster->lod_count - 1;

  return cluster->lods[level];
}

void BLB_ObjectAggregation3D_RebuildCluster(BLB_ObjectAggregation3D *aggregation, BLB_RenderObjects3DCluster *cluster) {
  if (!aggregation || !cluster)
    return;

  cluster->dirty = true;
  cluster->rendered = false;

  build_cluster(aggregation, cluster);
}
