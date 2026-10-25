#include "bulba/core/render/object_lod.h"

#include "bulba/core/utils/config.h"
#include "optimization/mesh_simplification.h"
#include "optimization/render_async.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define BLB_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define BLB_MKDIR(path) mkdir(path, 0755)
#endif

#define BLB_LOD_CACHE_MIN_CAPACITY 64u
#define BLB_LOD_CACHE_LOAD_NUMERATOR 7u
#define BLB_LOD_CACHE_LOAD_DENOMINATOR 10u
#define BLB_LOD_CACHE_EVICT_AGE 2u
#define BLB_LOD_DISK_VERSION 7u

typedef struct {
  bool occupied;
  bool is_2d;
  uint64_t key_hash;
  uint64_t geometry_hash;
  uint32_t error_bits;
  uint32_t reduction_bits;
  int level;
  uint32_t ref_count;
  uint64_t last_used_frame;
  size_t bytes;
  const BLB_Polygon3D *base3d;
  const BLB_Polygon2D *base2d;
  union {
    BLB_Polygon3D *polygon3d;
    BLB_Polygon2D *polygon2d;
  } polygon;
} BLB_ObjectLODCacheEntry;

static struct {
  BLB_ObjectLODCacheEntry *entries;
  size_t capacity;
  size_t count;
  uint64_t current_frame;
  uint64_t hits;
  uint64_t misses;
  size_t live_refs;
  size_t bytes;
} blb_lod_cache;

typedef struct {
  char magic[8];
  uint32_t version;
  uint8_t is_2d;
  uint8_t has_uvs;
  uint16_t reserved;
  int32_t level;
  uint64_t geometry_hash;
  uint32_t error_bits;
  uint32_t reduction_bits;
  uint64_t vertex_count;
  uint64_t index_count;
} BLB_LODDiskHeader;
static bool lod_eligible_3d(const BLB_Polygon3D *polygon) {
  if (!polygon || polygon->index_count < 3)
    return false;
  size_t triangles = polygon->index_count / 3u;

  return BLB_OBJECT_LOD_MIN_TRIANGLES <= 0 || triangles > (size_t)BLB_OBJECT_LOD_MIN_TRIANGLES;
}

static uint64_t polygon_geometry_hash3d_uncached(const void *value);
static uint64_t polygon_geometry_hash2d_uncached(const void *value);

static uint64_t polygon_geometry_hash3d(const BLB_Polygon3D *base);
static uint64_t polygon_geometry_hash2d(const BLB_Polygon2D *base);
static Mesh *mesh_from_polygon3d(const BLB_Polygon3D *polygon);
static Mesh *mesh_from_polygon2d(const BLB_Polygon2D *polygon);
static Mesh *simplify_lod_mesh(const Mesh *source);
static bool valid_level_request(int level);
static uint64_t make_key(bool is_2d, uint64_t geometry_hash, int level, uint32_t error_bits, uint32_t reduction_bits);
static BLB_ObjectLODCacheEntry *cache_find(bool is_2d, uint64_t geometry_hash, int level, uint32_t error_bits, uint32_t reduction_bits,
                                           uint64_t key_hash);
static bool cache_prepare_insert(size_t required_bytes);
static BLB_ObjectLODCacheEntry *cache_find_slot(uint64_t key_hash);

typedef struct BLB_AsyncLODJob {
  bool is_2d;
  uint64_t geometry_hash;
  uint32_t error_bits;
  uint32_t reduction_bits;
  int start_level;
  int target_level;
  int failed_level;
  uint64_t failure_key;
  const void *base;
  Mesh *source_mesh;
  union {
    BLB_Polygon3D **results3d;
    BLB_Polygon2D **results2d;
  } results;
  size_t result_count;
} BLB_AsyncLODJob;

typedef struct BLB_AsyncLODPending {
  bool is_2d;
  uint64_t geometry_hash;
  uint32_t error_bits;
  uint32_t reduction_bits;
  int start_level;
  int target_level;
  struct BLB_AsyncLODPending *next;
} BLB_AsyncLODPending;

static BLB_AsyncLODPending *blb_lod_pending = NULL;

static bool lod_pending_matches(const BLB_AsyncLODPending *entry, bool is_2d, uint64_t geometry_hash, int level, uint32_t error_bits,
                                uint32_t reduction_bits) {
  return entry && entry->is_2d == is_2d && entry->geometry_hash == geometry_hash && level >= entry->start_level && level <= entry->target_level &&
         entry->error_bits == error_bits && entry->reduction_bits == reduction_bits;
}

static bool lod_pending_contains(bool is_2d, uint64_t geometry_hash, int level, uint32_t error_bits, uint32_t reduction_bits) {
  for (BLB_AsyncLODPending *entry = blb_lod_pending; entry; entry = entry->next)
    if (lod_pending_matches(entry, is_2d, geometry_hash, level, error_bits, reduction_bits))
      return true;

  return false;
}

static BLB_AsyncLODPending *lod_pending_add(bool is_2d, uint64_t geometry_hash, int start_level, int target_level, uint32_t error_bits,
                                            uint32_t reduction_bits) {
  BLB_AsyncLODPending *entry = calloc(1, sizeof(*entry));

  if (!entry)
    return NULL;
  entry->is_2d = is_2d;
  entry->geometry_hash = geometry_hash;
  entry->start_level = start_level;
  entry->target_level = target_level;
  entry->error_bits = error_bits;
  entry->reduction_bits = reduction_bits;
  entry->next = blb_lod_pending;
  blb_lod_pending = entry;

  return entry;
}

static void lod_pending_remove(bool is_2d, uint64_t geometry_hash, int start_level, int target_level, uint32_t error_bits, uint32_t reduction_bits) {
  BLB_AsyncLODPending **link = &blb_lod_pending;

  while (*link) {
    BLB_AsyncLODPending *entry = *link;

    if (entry->is_2d == is_2d && entry->geometry_hash == geometry_hash && entry->start_level == start_level && entry->target_level == target_level &&
        entry->error_bits == error_bits && entry->reduction_bits == reduction_bits) {
      *link = entry->next;
      free(entry);

      return;
    }
    link = &entry->next;
  }
}

static void free_polygon3d_owned(BLB_Polygon3D *polygon) {
  if (!polygon)
    return;
  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->normals);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon);
}

static void free_polygon2d_owned(BLB_Polygon2D *polygon) {
  if (!polygon)
    return;
  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon);
}

static size_t polygon3d_owned_bytes(const BLB_Polygon3D *polygon) {
  if (!polygon)
    return 0;
  size_t bytes = sizeof(*polygon);

  if (polygon->vertex_count > SIZE_MAX / sizeof(*polygon->vertices))
    return SIZE_MAX;
  bytes += polygon->vertex_count * sizeof(*polygon->vertices);

  if (polygon->vertex_count > SIZE_MAX / sizeof(*polygon->base_vertices))
    return SIZE_MAX;
  bytes += polygon->vertex_count * sizeof(*polygon->base_vertices);

  if (polygon->vertex_count > SIZE_MAX / sizeof(*polygon->normals))
    return SIZE_MAX;
  bytes += polygon->vertex_count * sizeof(*polygon->normals);

  if (polygon->uvs) {
    if (polygon->vertex_count > SIZE_MAX / sizeof(*polygon->uvs))
      return SIZE_MAX;
    bytes += polygon->vertex_count * sizeof(*polygon->uvs);
  }

  if (polygon->index_count > SIZE_MAX / sizeof(*polygon->indices))
    return SIZE_MAX;
  bytes += polygon->index_count * sizeof(*polygon->indices);

  return bytes;
}

static size_t polygon2d_owned_bytes(const BLB_Polygon2D *polygon) {
  if (!polygon)
    return 0;
  size_t bytes = sizeof(*polygon);

  if (polygon->vertex_count > SIZE_MAX / sizeof(*polygon->vertices))
    return SIZE_MAX;
  bytes += polygon->vertex_count * sizeof(*polygon->vertices);

  if (polygon->vertex_count > SIZE_MAX / sizeof(*polygon->base_vertices))
    return SIZE_MAX;
  bytes += polygon->vertex_count * sizeof(*polygon->base_vertices);

  if (polygon->uvs) {
    if (polygon->vertex_count > SIZE_MAX / sizeof(*polygon->uvs))
      return SIZE_MAX;
    bytes += polygon->vertex_count * sizeof(*polygon->uvs);
  }

  if (polygon->index_count > SIZE_MAX / sizeof(*polygon->indices))
    return SIZE_MAX;
  bytes += polygon->index_count * sizeof(*polygon->indices);

  return bytes;
}

static void normalize_or_default3(BLB_Vec3 *value) {
  if (!value)
    return;
  float length = BLB_LenV3(*value);

  if (length > 0.000001f && isfinite(length))
    *value = BLB_MulV3F(*value, 1.0f / length);
  else
    *value = BLB_V3(0.0f, 1.0f, 0.0f);
}

static BLB_Polygon3D *polygon3d_from_mesh(const Mesh *mesh) {
  if (!mesh || !mesh->vertices || !mesh->indices || mesh->vertex_count == 0 || mesh->index_count < 3)
    return NULL;

  BLB_Polygon3D *polygon = calloc(1, sizeof(*polygon));

  if (!polygon)
    return NULL;

  polygon->vertices = malloc(mesh->vertex_count * sizeof(*polygon->vertices));
  polygon->base_vertices = malloc(mesh->vertex_count * sizeof(*polygon->base_vertices));
  polygon->normals = malloc(mesh->vertex_count * sizeof(*polygon->normals));
  polygon->indices = malloc(mesh->index_count * sizeof(*polygon->indices));

  if (mesh->uvs)
    polygon->uvs = malloc(mesh->vertex_count * sizeof(*polygon->uvs));

  if (!polygon->vertices || !polygon->base_vertices || !polygon->normals || !polygon->indices || (mesh->uvs && !polygon->uvs)) {
    free_polygon3d_owned(polygon);

    return NULL;
  }

  memcpy(polygon->vertices, mesh->vertices, mesh->vertex_count * sizeof(*polygon->vertices));
  memcpy(polygon->base_vertices, mesh->vertices, mesh->vertex_count * sizeof(*polygon->base_vertices));
  memcpy(polygon->indices, mesh->indices, mesh->index_count * sizeof(*polygon->indices));

  if (mesh->uvs)
    memcpy(polygon->uvs, mesh->uvs, mesh->vertex_count * sizeof(*polygon->uvs));

  memset(polygon->normals, 0, mesh->vertex_count * sizeof(*polygon->normals));

  for (size_t i = 0; i + 2 < mesh->index_count; i += 3) {
    unsigned int ia = mesh->indices[i];
    unsigned int ib = mesh->indices[i + 1];
    unsigned int ic = mesh->indices[i + 2];

    if (ia >= mesh->vertex_count || ib >= mesh->vertex_count || ic >= mesh->vertex_count) {
      free_polygon3d_owned(polygon);

      return NULL;
    }
    BLB_Vec3 normal = BLB_Cross(BLB_SubV3(mesh->vertices[ib], mesh->vertices[ia]), BLB_SubV3(mesh->vertices[ic], mesh->vertices[ia]));
    polygon->normals[ia] = BLB_AddV3(polygon->normals[ia], normal);
    polygon->normals[ib] = BLB_AddV3(polygon->normals[ib], normal);
    polygon->normals[ic] = BLB_AddV3(polygon->normals[ic], normal);
  }

  for (size_t i = 0; i < mesh->vertex_count; ++i)
    normalize_or_default3(&polygon->normals[i]);

  polygon->vertex_count = mesh->vertex_count;
  polygon->index_count = mesh->index_count;
  polygon->normal = BLB_V3(0.0f, 1.0f, 0.0f);

  if (mesh->index_count >= 3) {
    unsigned int ia = mesh->indices[0];
    unsigned int ib = mesh->indices[1];
    unsigned int ic = mesh->indices[2];

    if (ia < mesh->vertex_count && ib < mesh->vertex_count && ic < mesh->vertex_count) {
      polygon->normal = BLB_Cross(BLB_SubV3(mesh->vertices[ib], mesh->vertices[ia]), BLB_SubV3(mesh->vertices[ic], mesh->vertices[ia]));
      normalize_or_default3(&polygon->normal);
    }
  }

  return polygon;
}

static Mesh *mesh_from_polygon3d(const BLB_Polygon3D *polygon) {
  if (!polygon || !polygon->vertices || !polygon->indices)
    return NULL;
  Mesh *mesh = calloc(1, sizeof(*mesh));

  if (!mesh)
    return NULL;
  mesh->vertex_count = polygon->vertex_count;
  mesh->index_count = polygon->index_count;
  mesh->vertices = malloc(mesh->vertex_count * sizeof(*mesh->vertices));
  mesh->indices = malloc(mesh->index_count * sizeof(*mesh->indices));

  if (polygon->normals)
    mesh->normals = malloc(mesh->vertex_count * sizeof(*mesh->normals));

  if (polygon->uvs)
    mesh->uvs = malloc(mesh->vertex_count * sizeof(*mesh->uvs));

  if (!mesh->vertices || !mesh->indices || (polygon->normals && !mesh->normals) || (polygon->uvs && !mesh->uvs)) {
    BLB_MeshDestroy(mesh);

    return NULL;
  }
  memcpy(mesh->vertices, polygon->vertices, mesh->vertex_count * sizeof(*mesh->vertices));
  memcpy(mesh->indices, polygon->indices, mesh->index_count * sizeof(*mesh->indices));

  if (polygon->normals)
    memcpy(mesh->normals, polygon->normals, mesh->vertex_count * sizeof(*mesh->normals));

  if (polygon->uvs)
    memcpy(mesh->uvs, polygon->uvs, mesh->vertex_count * sizeof(*mesh->uvs));

  return mesh;
}

static BLB_Polygon2D *polygon2d_from_mesh(const Mesh *mesh) {
  if (!mesh || !mesh->vertices || !mesh->indices || mesh->vertex_count == 0 || mesh->index_count < 3)
    return NULL;
  BLB_Polygon2D *polygon = calloc(1, sizeof(*polygon));

  if (!polygon)
    return NULL;
  polygon->vertices = malloc(mesh->vertex_count * sizeof(*polygon->vertices));
  polygon->base_vertices = malloc(mesh->vertex_count * sizeof(*polygon->base_vertices));
  polygon->indices = malloc(mesh->index_count * sizeof(*polygon->indices));

  if (mesh->uvs)
    polygon->uvs = malloc(mesh->vertex_count * sizeof(*polygon->uvs));

  if (!polygon->vertices || !polygon->base_vertices || !polygon->indices || (mesh->uvs && !polygon->uvs)) {
    free_polygon2d_owned(polygon);

    return NULL;
  }

  for (size_t i = 0; i < mesh->vertex_count; ++i) {
    polygon->vertices[i] = BLB_V2(mesh->vertices[i].x, mesh->vertices[i].y);
    polygon->base_vertices[i] = polygon->vertices[i];
  }
  memcpy(polygon->indices, mesh->indices, mesh->index_count * sizeof(*polygon->indices));

  if (mesh->uvs)
    memcpy(polygon->uvs, mesh->uvs, mesh->vertex_count * sizeof(*polygon->uvs));
  polygon->vertex_count = mesh->vertex_count;
  polygon->index_count = mesh->index_count;

  return polygon;
}

static Mesh *mesh_from_polygon2d(const BLB_Polygon2D *polygon) {
  if (!polygon || !polygon->vertices || !polygon->indices)
    return NULL;
  Mesh *mesh = calloc(1, sizeof(*mesh));

  if (!mesh)
    return NULL;
  mesh->vertex_count = polygon->vertex_count;
  mesh->index_count = polygon->index_count;
  mesh->vertices = malloc(mesh->vertex_count * sizeof(*mesh->vertices));
  mesh->indices = malloc(mesh->index_count * sizeof(*mesh->indices));

  if (polygon->uvs)
    mesh->uvs = malloc(mesh->vertex_count * sizeof(*mesh->uvs));

  if (!mesh->vertices || !mesh->indices || (polygon->uvs && !mesh->uvs)) {
    BLB_MeshDestroy(mesh);

    return NULL;
  }

  for (size_t i = 0; i < mesh->vertex_count; ++i)
    mesh->vertices[i] = BLB_V3(polygon->vertices[i].x, polygon->vertices[i].y, 0.0f);
  memcpy(mesh->indices, polygon->indices, mesh->index_count * sizeof(*mesh->indices));

  if (polygon->uvs)
    memcpy(mesh->uvs, polygon->uvs, mesh->vertex_count * sizeof(*mesh->uvs));

  return mesh;
}

static bool lod_disk_path(char *path, size_t size, bool is_2d, uint64_t geometry_hash, int level, uint32_t error_bits, uint32_t reduction_bits) {
  if (!path || size == 0)
    return false;

  if (!BLB_DEFAULT_CACHE_PATH[0])
    BLB_InitConfig();

  if (!BLB_DEFAULT_CACHE_PATH[0])
    return false;

  char directory[BLB_PATH_MAX + 32];
  int written = snprintf(directory, sizeof(directory), "%s/lod_cache", BLB_DEFAULT_CACHE_PATH);

  if (written <= 0 || (size_t)written >= sizeof(directory))
    return false;

  BLB_MKDIR(directory);

  written = snprintf(path, size, "%s/%016llx_%08x_%08x_%d_%u.lod", directory, (unsigned long long)geometry_hash, error_bits, reduction_bits, level,
                     is_2d ? 1u : 0u);

  return written > 0 && (size_t)written < size;
}

static Mesh *load_lod_disk_mesh(bool is_2d, uint64_t geometry_hash, int level, uint32_t error_bits, uint32_t reduction_bits) {
  char path[BLB_PATH_MAX + 128];

  if (!lod_disk_path(path, sizeof(path), is_2d, geometry_hash, level, error_bits, reduction_bits))
    return NULL;

  FILE *file = fopen(path, "rb");

  if (!file)
    return NULL;

  BLB_LODDiskHeader header = {0};

  if (fread(&header, sizeof(header), 1, file) != 1 || memcmp(header.magic, "BULBLOD1", 8) != 0 || header.version != BLB_LOD_DISK_VERSION ||
      header.is_2d != (is_2d ? 1u : 0u) || header.level != level || header.geometry_hash != geometry_hash || header.error_bits != error_bits ||
      header.reduction_bits != reduction_bits || header.vertex_count == 0 || header.index_count < 3 || header.index_count % 3u != 0 ||
      header.vertex_count > SIZE_MAX / sizeof(BLB_Vec3) || header.index_count > SIZE_MAX / sizeof(unsigned int)) {
    fclose(file);

    return NULL;
  }

  Mesh *mesh = calloc(1, sizeof(*mesh));

  if (!mesh) {
    fclose(file);

    return NULL;
  }

  mesh->vertex_count = (size_t)header.vertex_count;
  mesh->index_count = (size_t)header.index_count;
  mesh->vertices = malloc(mesh->vertex_count * sizeof(*mesh->vertices));
  mesh->indices = malloc(mesh->index_count * sizeof(*mesh->indices));

  if (header.has_uvs)
    mesh->uvs = malloc(mesh->vertex_count * sizeof(*mesh->uvs));

  if (!mesh->vertices || !mesh->indices || (header.has_uvs && !mesh->uvs) ||
      fread(mesh->vertices, sizeof(*mesh->vertices), mesh->vertex_count, file) != mesh->vertex_count ||
      fread(mesh->indices, sizeof(*mesh->indices), mesh->index_count, file) != mesh->index_count ||
      (header.has_uvs && fread(mesh->uvs, sizeof(*mesh->uvs), mesh->vertex_count, file) != mesh->vertex_count)) {
    BLB_MeshDestroy(mesh);
    fclose(file);

    return NULL;
  }

  fclose(file);

  for (size_t i = 0; i < mesh->vertex_count; ++i) {
    BLB_Vec3 vertex = mesh->vertices[i];

    if (!isfinite(vertex.x) || !isfinite(vertex.y) || !isfinite(vertex.z)) {
      BLB_MeshDestroy(mesh);

      return NULL;
    }

    if (mesh->uvs && (!isfinite(mesh->uvs[i].x) || !isfinite(mesh->uvs[i].y))) {
      BLB_MeshDestroy(mesh);

      return NULL;
    }
  }

  for (size_t i = 0; i < mesh->index_count; ++i) {
    if (mesh->indices[i] >= mesh->vertex_count) {
      BLB_MeshDestroy(mesh);

      return NULL;
    }
  }

  return mesh;
}

static void save_lod_disk_mesh(bool is_2d, uint64_t geometry_hash, int level, uint32_t error_bits, uint32_t reduction_bits, const Mesh *mesh) {
  if (!mesh || !mesh->vertices || !mesh->indices || mesh->vertex_count == 0 || mesh->index_count < 3)
    return;

  char path[BLB_PATH_MAX + 128];

  if (!lod_disk_path(path, sizeof(path), is_2d, geometry_hash, level, error_bits, reduction_bits))
    return;

  char temp_path[BLB_PATH_MAX + 140];
  int written = snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);

  if (written <= 0 || (size_t)written >= sizeof(temp_path))
    return;

  FILE *file = fopen(temp_path, "wb");

  if (!file)
    return;

  BLB_LODDiskHeader header = {0};
  memcpy(header.magic, "BULBLOD1", 8);
  header.version = BLB_LOD_DISK_VERSION;
  header.is_2d = is_2d ? 1u : 0u;
  header.has_uvs = mesh->uvs ? 1u : 0u;
  header.level = level;
  header.geometry_hash = geometry_hash;
  header.error_bits = error_bits;
  header.reduction_bits = reduction_bits;
  header.vertex_count = mesh->vertex_count;
  header.index_count = mesh->index_count;

  bool ok = fwrite(&header, sizeof(header), 1, file) == 1 &&
            fwrite(mesh->vertices, sizeof(*mesh->vertices), mesh->vertex_count, file) == mesh->vertex_count &&
            fwrite(mesh->indices, sizeof(*mesh->indices), mesh->index_count, file) == mesh->index_count;

  if (ok && header.has_uvs)
    ok = fwrite(mesh->uvs, sizeof(*mesh->uvs), mesh->vertex_count, file) == mesh->vertex_count;

  if (fclose(file) != 0)
    ok = false;

  if (ok)
    rename(temp_path, path);
  else
    remove(temp_path);
}

static uint64_t hash_mix64(uint64_t value) {
  value ^= value >> 30;
  value *= UINT64_C(0xbf58476d1ce4e5b9);
  value ^= value >> 27;
  value *= UINT64_C(0x94d049bb133111eb);
  value ^= value >> 31;

  return value;
}

static uint32_t float_bits(float value) {
  uint32_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));

  return bits;
}

static uint64_t fnv1a64(const void *data, size_t bytes, uint64_t hash) {
  const unsigned char *p = (const unsigned char *)data;

  for (size_t i = 0; i < bytes; ++i) {
    hash ^= p[i];
    hash *= UINT64_C(1099511628211);
  }

  return hash;
}

static uint64_t polygon_geometry_hash3d_uncached(const void *value) {
  const BLB_Polygon3D *base = value;

  if (!base || !base->vertices || !base->indices)
    return 0;
  uint64_t h = UINT64_C(1469598103934665603);
  h = fnv1a64(&base->vertex_count, sizeof(base->vertex_count), h);
  h = fnv1a64(&base->index_count, sizeof(base->index_count), h);
  h = fnv1a64(base->vertices, base->vertex_count * sizeof(*base->vertices), h);
  h = fnv1a64(base->indices, base->index_count * sizeof(*base->indices), h);

  if (base->uvs)
    h = fnv1a64(base->uvs, base->vertex_count * sizeof(*base->uvs), h);

  return hash_mix64(h);
}

static uint64_t polygon_geometry_hash2d_uncached(const void *value) {
  const BLB_Polygon2D *base = value;

  if (!base || !base->vertices || !base->indices)
    return 0;

  uint64_t h = UINT64_C(1469598103934665603);
  h = fnv1a64(&base->vertex_count, sizeof(base->vertex_count), h);
  h = fnv1a64(&base->index_count, sizeof(base->index_count), h);
  h = fnv1a64(base->vertices, base->vertex_count * sizeof(*base->vertices), h);
  h = fnv1a64(base->indices, base->index_count * sizeof(*base->indices), h);

  if (base->uvs)
    h = fnv1a64(base->uvs, base->vertex_count * sizeof(*base->uvs), h);

  return hash_mix64(h);
}

static uint64_t polygon_geometry_hash3d(const BLB_Polygon3D *base) {
  if (!base)
    return 0;
  BLB_Polygon3D *mutable_base = (BLB_Polygon3D *)base;

  if (mutable_base->geometry_hash == 0)
    mutable_base->geometry_hash = polygon_geometry_hash3d_uncached(base);

  return mutable_base->geometry_hash;
}

static uint64_t polygon_geometry_hash2d(const BLB_Polygon2D *base) {
  if (!base)
    return 0;
  BLB_Polygon2D *mutable_base = (BLB_Polygon2D *)base;

  if (mutable_base->geometry_hash == 0)
    mutable_base->geometry_hash = polygon_geometry_hash2d_uncached(base);

  return mutable_base->geometry_hash;
}

static size_t next_power_of_two(size_t value) {
  size_t result = BLB_LOD_CACHE_MIN_CAPACITY;

  while (result < value) {
    if (result > SIZE_MAX / 2u)
      return 0;
    result <<= 1u;
  }

  return result;
}

static void cache_destroy_entry(BLB_ObjectLODCacheEntry *entry) {
  if (!entry || !entry->occupied)
    return;

  if (entry->is_2d)
    free_polygon2d_owned(entry->polygon.polygon2d);
  else
    free_polygon3d_owned(entry->polygon.polygon3d);

  if (blb_lod_cache.bytes >= entry->bytes)
    blb_lod_cache.bytes -= entry->bytes;
  else
    blb_lod_cache.bytes = 0;
  memset(entry, 0, sizeof(*entry));
}

static bool cache_grow(size_t min_capacity) {
  if (blb_lod_cache.capacity >= min_capacity)
    return true;
  size_t new_capacity = next_power_of_two(min_capacity);

  if (!new_capacity)
    return false;
  BLB_ObjectLODCacheEntry *entries = calloc(new_capacity, sizeof(*entries));

  if (!entries)
    return false;

  if (blb_lod_cache.entries) {
    size_t mask = new_capacity - 1u;

    for (size_t i = 0; i < blb_lod_cache.capacity; ++i) {
      BLB_ObjectLODCacheEntry *old = &blb_lod_cache.entries[i];

      if (!old->occupied)
        continue;
      size_t slot = (size_t)old->key_hash & mask;

      while (entries[slot].occupied)
        slot = (slot + 1u) & mask;
      entries[slot] = *old;
    }
  }

  free(blb_lod_cache.entries);
  blb_lod_cache.entries = entries;
  blb_lod_cache.capacity = new_capacity;

  return true;
}

static bool cache_key_matches(const BLB_ObjectLODCacheEntry *entry, bool is_2d, uint64_t geometry_hash, int level, uint32_t error_bits,
                              uint32_t reduction_bits) {
  return entry && entry->occupied && entry->is_2d == is_2d && entry->geometry_hash == geometry_hash && entry->level == level &&
         entry->error_bits == error_bits && entry->reduction_bits == reduction_bits;
}

static BLB_ObjectLODCacheEntry *cache_find(bool is_2d, uint64_t geometry_hash, int level, uint32_t error_bits, uint32_t reduction_bits,
                                           uint64_t key_hash) {
  if (!blb_lod_cache.entries || blb_lod_cache.capacity == 0)
    return NULL;
  size_t mask = blb_lod_cache.capacity - 1u;
  size_t slot = (size_t)key_hash & mask;

  for (size_t probes = 0; probes < blb_lod_cache.capacity; ++probes) {
    BLB_ObjectLODCacheEntry *entry = &blb_lod_cache.entries[slot];

    if (!entry->occupied)
      return NULL;

    if (entry->key_hash == key_hash && cache_key_matches(entry, is_2d, geometry_hash, level, error_bits, reduction_bits)) {
      entry->last_used_frame = blb_lod_cache.current_frame;

      return entry;
    }
    slot = (slot + 1u) & mask;
  }

  return NULL;
}

static BLB_ObjectLODCacheEntry *cache_find_slot(uint64_t key_hash) {
  size_t mask = blb_lod_cache.capacity - 1u;
  size_t slot = (size_t)key_hash & mask;

  while (blb_lod_cache.entries[slot].occupied)
    slot = (slot + 1u) & mask;

  return &blb_lod_cache.entries[slot];
}

static uint64_t make_key(bool is_2d, uint64_t geometry_hash, int level, uint32_t error_bits, uint32_t reduction_bits) {
  uint64_t key = geometry_hash;
  key ^= ((uint64_t)(uint32_t)level + UINT64_C(0x9e3779b9)) * UINT64_C(0x517cc1b727220a95);
  key ^= ((uint64_t)error_bits << 17) ^ ((uint64_t)reduction_bits << 41);
  key ^= BLB_OBJECT_LOD_PRESERVE_BOUNDARIES ? UINT64_C(0x632be59bd9b4e019) : 0;
  key ^= BLB_OBJECT_LOD_PRESERVE_NORMALS ? UINT64_C(0x8cb92ba72f3d5f17) : 0;

  if (is_2d)
    key ^= UINT64_C(0xd2b74407b1ce6e93);

  return hash_mix64(key);
}

static void cache_rehash(void) {
  if (!blb_lod_cache.entries || blb_lod_cache.capacity == 0)
    return;
  BLB_ObjectLODCacheEntry *entries = calloc(blb_lod_cache.capacity, sizeof(*entries));

  if (!entries)
    return;
  size_t mask = blb_lod_cache.capacity - 1u;

  for (size_t i = 0; i < blb_lod_cache.capacity; ++i) {
    BLB_ObjectLODCacheEntry *old = &blb_lod_cache.entries[i];

    if (!old->occupied)
      continue;
    size_t slot = (size_t)old->key_hash & mask;

    while (entries[slot].occupied)
      slot = (slot + 1u) & mask;
    entries[slot] = *old;
  }
  free(blb_lod_cache.entries);
  blb_lod_cache.entries = entries;
}

static void cache_remove_entry(BLB_ObjectLODCacheEntry *entry) {
  if (!entry || !entry->occupied)
    return;
  cache_destroy_entry(entry);

  if (blb_lod_cache.count > 0)
    --blb_lod_cache.count;
  cache_rehash();
}

static void cache_release(BLB_ObjectLODCacheEntry *entry) {
  if (!entry || !entry->occupied || entry->ref_count == 0)
    return;
  --entry->ref_count;

  if (blb_lod_cache.live_refs > 0)
    --blb_lod_cache.live_refs;
  entry->last_used_frame = blb_lod_cache.current_frame;
}

static bool cache_evict_one(bool force) {
  BLB_ObjectLODCacheEntry *victim = NULL;

  for (size_t i = 0; i < blb_lod_cache.capacity; ++i) {
    BLB_ObjectLODCacheEntry *entry = &blb_lod_cache.entries[i];

    if (!entry->occupied || entry->ref_count != 0)
      continue;
    uint64_t age = blb_lod_cache.current_frame - entry->last_used_frame;

    if (!force && age <= BLB_LOD_CACHE_EVICT_AGE)
      continue;

    if (!victim || entry->last_used_frame < victim->last_used_frame)
      victim = entry;
  }

  if (!victim)
    return false;
  cache_remove_entry(victim);

  return true;
}

static bool cache_prepare_insert(size_t required_bytes) {
  size_t max_entries = BLB_OBJECT_LOD_CACHE_MAX_ENTRIES > 0 ? (size_t)BLB_OBJECT_LOD_CACHE_MAX_ENTRIES : 1u;
  size_t max_bytes = BLB_OBJECT_LOD_CACHE_MAX_BYTES > 0 ? BLB_OBJECT_LOD_CACHE_MAX_BYTES : 1u;

  if (required_bytes > max_bytes)
    return false;

  while (blb_lod_cache.count >= max_entries || blb_lod_cache.bytes > max_bytes - required_bytes) {
    if (!cache_evict_one(false))
      return false;
  }

  if (!cache_grow(blb_lod_cache.count + 1u))
    return false;

  if (blb_lod_cache.count * BLB_LOD_CACHE_LOAD_DENOMINATOR >= blb_lod_cache.capacity * BLB_LOD_CACHE_LOAD_NUMERATOR)
    return cache_grow(blb_lod_cache.capacity + 1u);

  return true;
}

static bool valid_level_request(int level) {
  if (level < 0)
    return false;

  if (level == 0)
    return true;

  if (!BLB_OBJECT_LOD)
    return false;
  int max_levels = BLB_OBJECT_LOD_LEVELS > 0 ? BLB_OBJECT_LOD_LEVELS : 0;

  return level <= max_levels;
}

static int clamp_lod_level(int level) {
  int max_levels = BLB_OBJECT_LOD_LEVELS > 0 ? BLB_OBJECT_LOD_LEVELS : 0;

  if (level < 0)
    return 0;

  if (level > max_levels)
    return max_levels;

  return level;
}

static int resolve_level(bool override_enabled, int explicit_level, float distance) {
  if (!BLB_OBJECT_LOD)
    return 0;

  int maximum = clamp_lod_level(BLB_OBJECT_LOD_LEVELS);
  int minimum = clamp_lod_level(BLB_OBJECT_LOD_MIN_LEVEL);
  int level = 0;

  if (override_enabled) {
    level = clamp_lod_level(explicit_level);
  } else if (BLB_OBJECT_LOD_DISTANCE_OPTIMIZATION && BLB_OBJECT_LOD_DISTANCE_STEP > 0.0f && isfinite(distance) &&
             distance > BLB_OBJECT_LOD_DISTANCE_START) {
    int reductions = 1 + (int)floorf((distance - BLB_OBJECT_LOD_DISTANCE_START) / BLB_OBJECT_LOD_DISTANCE_STEP);
    level = reductions;

    if (level > maximum)
      level = maximum;
  }

  if (level < minimum)
    level = minimum;

  if (level > maximum)
    level = maximum;

  return level;
}

static bool lod_shape_preserved_3d(const Mesh *source, const Mesh *candidate) {
  if (!source || !candidate || !source->vertices || !candidate->vertices || source->vertex_count == 0 || candidate->vertex_count == 0)
    return false;

  BLB_Vec3 source_min = BLB_V3(FLT_MAX, FLT_MAX, FLT_MAX);
  BLB_Vec3 source_max = BLB_V3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  BLB_Vec3 candidate_min = BLB_V3(FLT_MAX, FLT_MAX, FLT_MAX);
  BLB_Vec3 candidate_max = BLB_V3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  for (size_t i = 0; i < source->vertex_count; ++i) {
    BLB_Vec3 v = source->vertices[i];
    source_min.x = fminf(source_min.x, v.x);
    source_min.y = fminf(source_min.y, v.y);
    source_min.z = fminf(source_min.z, v.z);
    source_max.x = fmaxf(source_max.x, v.x);
    source_max.y = fmaxf(source_max.y, v.y);
    source_max.z = fmaxf(source_max.z, v.z);
  }

  for (size_t i = 0; i < candidate->vertex_count; ++i) {
    BLB_Vec3 v = candidate->vertices[i];
    candidate_min.x = fminf(candidate_min.x, v.x);
    candidate_min.y = fminf(candidate_min.y, v.y);
    candidate_min.z = fminf(candidate_min.z, v.z);
    candidate_max.x = fmaxf(candidate_max.x, v.x);
    candidate_max.y = fmaxf(candidate_max.y, v.y);
    candidate_max.z = fmaxf(candidate_max.z, v.z);
  }

  float source_extent[3] = {source_max.x - source_min.x, source_max.y - source_min.y, source_max.z - source_min.z};
  float candidate_extent[3] = {candidate_max.x - candidate_min.x, candidate_max.y - candidate_min.y, candidate_max.z - candidate_min.z};
  float source_center[3] = {(source_max.x + source_min.x) * 0.5f, (source_max.y + source_min.y) * 0.5f, (source_max.z + source_min.z) * 0.5f};
  float candidate_center[3] = {(candidate_max.x + candidate_min.x) * 0.5f, (candidate_max.y + candidate_min.y) * 0.5f,
                               (candidate_max.z + candidate_min.z) * 0.5f};

  for (int axis = 0; axis < 3; ++axis) {
    if (!isfinite(source_extent[axis]) || !isfinite(candidate_extent[axis]))
      return false;

    if (source_extent[axis] > 0.000001f && candidate_extent[axis] < source_extent[axis] * 0.45f)
      return false;
    float center_limit = fmaxf(source_extent[axis] * 0.25f, 0.001f);

    if (fabsf(candidate_center[axis] - source_center[axis]) > center_limit)
      return false;
  }

  return true;
}

static bool lod_shape_preserved_2d(const Mesh *source, const Mesh *candidate) {
  if (!source || !candidate || !source->vertices || !candidate->vertices || source->vertex_count == 0 || candidate->vertex_count == 0)
    return false;
  BLB_Vec2 source_min = BLB_V2(FLT_MAX, FLT_MAX), source_max = BLB_V2(-FLT_MAX, -FLT_MAX);
  BLB_Vec2 candidate_min = BLB_V2(FLT_MAX, FLT_MAX), candidate_max = BLB_V2(-FLT_MAX, -FLT_MAX);

  for (size_t i = 0; i < source->vertex_count; ++i) {
    BLB_Vec3 v = source->vertices[i];
    source_min.x = fminf(source_min.x, v.x);
    source_min.y = fminf(source_min.y, v.y);
    source_max.x = fmaxf(source_max.x, v.x);
    source_max.y = fmaxf(source_max.y, v.y);
  }

  for (size_t i = 0; i < candidate->vertex_count; ++i) {
    BLB_Vec3 v = candidate->vertices[i];
    candidate_min.x = fminf(candidate_min.x, v.x);
    candidate_min.y = fminf(candidate_min.y, v.y);
    candidate_max.x = fmaxf(candidate_max.x, v.x);
    candidate_max.y = fmaxf(candidate_max.y, v.y);
  }
  float source_extent[2] = {source_max.x - source_min.x, source_max.y - source_min.y};
  float candidate_extent[2] = {candidate_max.x - candidate_min.x, candidate_max.y - candidate_min.y};

  for (int axis = 0; axis < 2; ++axis)
    if (source_extent[axis] > 0.000001f && candidate_extent[axis] < source_extent[axis] * 0.45f)
      return false;

  return true;
}

static Mesh *simplify_lod_mesh(const Mesh *source) {
  if (!source)
    return NULL;

  float error = fmaxf(BLB_OBJECT_LOD_ERROR, 0.000001f);
  float reduction = fminf(fmaxf(BLB_OBJECT_LOD_REDUCTION, 0.10f), 0.95f);
  Mesh *candidate = BLB_MeshSimplifyQEMWithReduction(source, error, reduction);

  if (candidate && candidate->index_count < source->index_count && lod_shape_preserved_3d(source, candidate))
    return candidate;
  BLB_MeshDestroy(candidate);

  float fallback_error = fmaxf(BLB_OBJECT_LOD_FALLBACK_ERROR, error);

  if (fallback_error <= error)
    return NULL;

  float fallback_reduction = fmaxf(reduction, 0.65f);
  fallback_reduction = fminf(fallback_reduction, 0.95f);
  candidate = BLB_MeshSimplifyQEMWithReduction(source, fallback_error, fallback_reduction);

  if (candidate && candidate->index_count < source->index_count && lod_shape_preserved_3d(source, candidate))
    return candidate;
  BLB_MeshDestroy(candidate);

  return NULL;
}

static BLB_Polygon3D *generate_lod3d(const BLB_Polygon3D *source) {
  Mesh *source_mesh = mesh_from_polygon3d(source);

  if (!source_mesh)
    return NULL;
  Mesh *simplified = simplify_lod_mesh(source_mesh);
  BLB_MeshDestroy(source_mesh);

  if (!simplified)
    return NULL;
  BLB_Polygon3D *lod = polygon3d_from_mesh(simplified);
  BLB_MeshDestroy(simplified);

  return lod;
}

static BLB_Polygon2D *generate_lod2d(const BLB_Polygon2D *source) {
  Mesh *source_mesh = mesh_from_polygon2d(source);

  if (!source_mesh)
    return NULL;
  Mesh *simplified = simplify_lod_mesh(source_mesh);
  BLB_MeshDestroy(source_mesh);

  if (!simplified)
    return NULL;
  Mesh *base_mesh = mesh_from_polygon2d(source);

  if (!base_mesh) {
    BLB_MeshDestroy(simplified);

    return NULL;
  }
  bool preserved = lod_shape_preserved_2d(base_mesh, simplified);
  BLB_MeshDestroy(base_mesh);

  if (!preserved) {
    BLB_MeshDestroy(simplified);

    return NULL;
  }
  BLB_Polygon2D *lod = polygon2d_from_mesh(simplified);
  BLB_MeshDestroy(simplified);

  return lod;
}

static BLB_ObjectLODCacheEntry *cache_insert_generated_3d(const BLB_Polygon3D *base, int level, uint64_t geometry_hash, uint32_t error_bits,
                                                          uint32_t reduction_bits, BLB_Polygon3D *lod) {
  if (!base || !lod || !valid_level_request(level))
    return NULL;

  uint64_t key_hash = make_key(false, geometry_hash, level, error_bits, reduction_bits);
  BLB_ObjectLODCacheEntry *existing = cache_find(false, geometry_hash, level, error_bits, reduction_bits, key_hash);

  if (existing) {
    free_polygon3d_owned(lod);
    existing->last_used_frame = blb_lod_cache.current_frame;

    return existing;
  }

  size_t bytes = polygon3d_owned_bytes(lod);

  if (!cache_prepare_insert(bytes)) {
    free_polygon3d_owned(lod);

    return NULL;
  }

  BLB_ObjectLODCacheEntry *entry = cache_find_slot(key_hash);
  entry->occupied = true;
  entry->is_2d = false;
  entry->key_hash = key_hash;
  entry->geometry_hash = geometry_hash;
  entry->error_bits = error_bits;
  entry->reduction_bits = reduction_bits;
  entry->level = level;
  entry->ref_count = 0;
  entry->last_used_frame = blb_lod_cache.current_frame;
  entry->bytes = bytes;
  entry->base3d = base;
  entry->polygon.polygon3d = lod;
  ++blb_lod_cache.count;
  blb_lod_cache.bytes += bytes;

  return entry;
}

static BLB_ObjectLODCacheEntry *cache_insert_generated_2d(const BLB_Polygon2D *base, int level, uint64_t geometry_hash, uint32_t error_bits,
                                                          uint32_t reduction_bits, BLB_Polygon2D *lod) {
  if (!base || !lod || !valid_level_request(level))
    return NULL;

  uint64_t key_hash = make_key(true, geometry_hash, level, error_bits, reduction_bits);
  BLB_ObjectLODCacheEntry *existing = cache_find(true, geometry_hash, level, error_bits, reduction_bits, key_hash);

  if (existing) {
    free_polygon2d_owned(lod);
    existing->last_used_frame = blb_lod_cache.current_frame;

    return existing;
  }

  size_t bytes = polygon2d_owned_bytes(lod);

  if (!cache_prepare_insert(bytes)) {
    free_polygon2d_owned(lod);

    return NULL;
  }

  BLB_ObjectLODCacheEntry *entry = cache_find_slot(key_hash);
  entry->occupied = true;
  entry->is_2d = true;
  entry->key_hash = key_hash;
  entry->geometry_hash = geometry_hash;
  entry->error_bits = error_bits;
  entry->reduction_bits = reduction_bits;
  entry->level = level;
  entry->ref_count = 0;
  entry->last_used_frame = blb_lod_cache.current_frame;
  entry->bytes = bytes;
  entry->base2d = base;
  entry->polygon.polygon2d = lod;
  ++blb_lod_cache.count;
  blb_lod_cache.bytes += bytes;

  return entry;
}

static uint64_t lod_failed_keys[256];
static size_t lod_failed_cursor;

static uint64_t lod_failure_key(bool is_2d, uint64_t geometry_hash, int level, uint32_t error_bits, uint32_t reduction_bits) {
  uint64_t key = make_key(is_2d, geometry_hash, level, error_bits, reduction_bits);
  key ^= (uint64_t)float_bits(BLB_OBJECT_LOD_FALLBACK_ERROR) << 17u;
  key ^= (uint64_t)(uint32_t)BLB_OBJECT_LOD_MIN_TRIANGLES << 33u;

  return key ? key : 1u;
}

static bool lod_failure_known(uint64_t key) {
  for (size_t i = 0; i < sizeof(lod_failed_keys) / sizeof(lod_failed_keys[0]); ++i)
    if (lod_failed_keys[i] == key)
      return true;

  return false;
}

static void async_lod_run(void *user_data) {
  BLB_AsyncLODJob *job = user_data;

  if (!job || !job->source_mesh || job->start_level <= 0 || job->target_level < job->start_level)
    return;

  job->result_count = (size_t)(job->target_level - job->start_level + 1);

  if (job->is_2d)
    job->results.results2d = calloc(job->result_count, sizeof(*job->results.results2d));
  else
    job->results.results3d = calloc(job->result_count, sizeof(*job->results.results3d));

  if ((job->is_2d && !job->results.results2d) || (!job->is_2d && !job->results.results3d)) {
    job->result_count = 0;

    return;
  }

  Mesh *current = job->source_mesh;
  bool current_owned = false;

  for (int level = job->start_level; level <= job->target_level; ++level) {
    if (!job->is_2d && BLB_OBJECT_LOD_MIN_TRIANGLES > 0 && current->index_count / 3u <= (size_t)BLB_OBJECT_LOD_MIN_TRIANGLES)
      break;
    Mesh *next = load_lod_disk_mesh(job->is_2d, job->geometry_hash, level, job->error_bits, job->reduction_bits);

    if (next && ((!job->is_2d && !lod_shape_preserved_3d(current, next)) || (job->is_2d && !lod_shape_preserved_2d(current, next)))) {
      BLB_MeshDestroy(next);
      next = NULL;
    }

    if (!next) {
      next = simplify_lod_mesh(current);

      if (!next) {
        job->failed_level = level;
        break;
      }
      save_lod_disk_mesh(job->is_2d, job->geometry_hash, level, job->error_bits, job->reduction_bits, next);
    }

    size_t index = (size_t)(level - job->start_level);

    if (job->is_2d)
      job->results.results2d[index] = polygon2d_from_mesh(next);
    else
      job->results.results3d[index] = polygon3d_from_mesh(next);

    if (current_owned)
      BLB_MeshDestroy(current);
    current = next;
    current_owned = true;
  }

  if (current_owned)
    BLB_MeshDestroy(current);
}

static void async_lod_complete(void *user_data) {
  BLB_AsyncLODJob *job = user_data;

  if (!job)
    return;

  if (job->failed_level > 0) {
    uint64_t key = job->failure_key;

    if (job->failed_level != job->start_level)
      key = lod_failure_key(job->is_2d, job->geometry_hash, job->failed_level, job->error_bits, job->reduction_bits);
    lod_failed_keys[lod_failed_cursor++ % (sizeof(lod_failed_keys) / sizeof(lod_failed_keys[0]))] = key;
  }

  for (size_t i = 0; i < job->result_count; ++i) {
    int level = job->start_level + (int)i;

    if (job->is_2d) {
      BLB_Polygon2D *polygon = job->results.results2d ? job->results.results2d[i] : NULL;

      if (polygon) {
        cache_insert_generated_2d((const BLB_Polygon2D *)job->base, level, job->geometry_hash, job->error_bits, job->reduction_bits, polygon);
        job->results.results2d[i] = NULL;
      }
    } else {
      BLB_Polygon3D *polygon = job->results.results3d ? job->results.results3d[i] : NULL;

      if (polygon) {
        cache_insert_generated_3d((const BLB_Polygon3D *)job->base, level, job->geometry_hash, job->error_bits, job->reduction_bits, polygon);
        job->results.results3d[i] = NULL;
      }
    }
  }

  lod_pending_remove(job->is_2d, job->geometry_hash, job->start_level, job->target_level, job->error_bits, job->reduction_bits);
}

static void async_lod_destroy(void *user_data) {
  BLB_AsyncLODJob *job = user_data;

  if (!job)
    return;
  BLB_MeshDestroy(job->source_mesh);

  if (job->is_2d) {
    if (job->results.results2d) {
      for (size_t i = 0; i < job->result_count; ++i)
        free_polygon2d_owned(job->results.results2d[i]);
      free(job->results.results2d);
    }
  } else {
    if (job->results.results3d) {
      for (size_t i = 0; i < job->result_count; ++i)
        free_polygon3d_owned(job->results.results3d[i]);
      free(job->results.results3d);
    }
  }
  lod_pending_remove(job->is_2d, job->geometry_hash, job->start_level, job->target_level, job->error_bits, job->reduction_bits);
  free(job);
}

static bool schedule_async_lod3d(const BLB_Polygon3D *base, const BLB_Polygon3D *source, int start_level, int target_level) {
  if (!base || !source || start_level <= 0 || target_level < start_level || !BLB_OBJECT_LOD_CACHE || !BLB_OBJECT_LOD)
    return false;

  uint64_t geometry_hash = polygon_geometry_hash3d(base);
  uint32_t error_bits = float_bits(BLB_OBJECT_LOD_ERROR);
  uint32_t reduction_bits = float_bits(BLB_OBJECT_LOD_REDUCTION);
  uint64_t failure_key = lod_failure_key(false, geometry_hash, start_level, error_bits, reduction_bits);

  if (lod_failure_known(failure_key) || lod_pending_contains(false, geometry_hash, start_level, error_bits, reduction_bits))
    return false;

  Mesh *source_mesh = mesh_from_polygon3d(source);

  if (!source_mesh)
    return false;

  BLB_AsyncLODJob *job = calloc(1, sizeof(*job));

  if (!job) {
    BLB_MeshDestroy(source_mesh);

    return false;
  }

  job->is_2d = false;
  job->geometry_hash = geometry_hash;
  job->error_bits = error_bits;
  job->reduction_bits = reduction_bits;
  job->start_level = start_level;
  job->target_level = target_level;
  job->failure_key = failure_key;
  job->base = base;
  job->source_mesh = source_mesh;

  if (!lod_pending_add(false, geometry_hash, start_level, target_level, error_bits, reduction_bits) ||
      BLB_RenderAsync_Submit(async_lod_run, async_lod_complete, async_lod_destroy, job) != 0) {
    lod_pending_remove(false, geometry_hash, start_level, target_level, error_bits, reduction_bits);
    async_lod_destroy(job);

    return false;
  }

  return false;
}

static bool schedule_async_lod2d(const BLB_Polygon2D *base, const BLB_Polygon2D *source, int start_level, int target_level) {
  if (!base || !source || start_level <= 0 || target_level < start_level || !BLB_OBJECT_LOD_CACHE || !BLB_OBJECT_LOD)
    return false;

  uint64_t geometry_hash = polygon_geometry_hash2d(base);
  uint32_t error_bits = float_bits(BLB_OBJECT_LOD_ERROR);
  uint32_t reduction_bits = float_bits(BLB_OBJECT_LOD_REDUCTION);
  uint64_t failure_key = lod_failure_key(true, geometry_hash, start_level, error_bits, reduction_bits);

  if (lod_failure_known(failure_key) || lod_pending_contains(true, geometry_hash, start_level, error_bits, reduction_bits))
    return false;

  Mesh *source_mesh = mesh_from_polygon2d(source);

  if (!source_mesh)
    return false;

  BLB_AsyncLODJob *job = calloc(1, sizeof(*job));

  if (!job) {
    BLB_MeshDestroy(source_mesh);

    return false;
  }

  job->is_2d = true;
  job->geometry_hash = geometry_hash;
  job->error_bits = error_bits;
  job->reduction_bits = reduction_bits;
  job->start_level = start_level;
  job->target_level = target_level;
  job->failure_key = failure_key;
  job->base = base;
  job->source_mesh = source_mesh;

  if (!lod_pending_add(true, geometry_hash, start_level, target_level, error_bits, reduction_bits) ||
      BLB_RenderAsync_Submit(async_lod_run, async_lod_complete, async_lod_destroy, job) != 0) {
    lod_pending_remove(true, geometry_hash, start_level, target_level, error_bits, reduction_bits);
    async_lod_destroy(job);

    return false;
  }

  return false;
}

static bool ensure_array3d(BLB_Object3D *object, int level) {
  if (!object || level <= 0)
    return true;
  size_t target = (size_t)level;

  if (object->lod_polygon_count >= target)
    return true;
  BLB_Polygon3D **new_lods = realloc(object->lod_polygons, target * sizeof(*new_lods));

  if (!new_lods)
    return false;

  for (size_t i = object->lod_polygon_count; i < target; ++i)
    new_lods[i] = NULL;
  object->lod_polygons = new_lods;
  object->lod_polygon_count = target;

  return true;
}

static bool ensure_array2d(BLB_Object2D *object, int level) {
  if (!object || level <= 0)
    return true;
  size_t target = (size_t)level;

  if (object->lod_polygon_count >= target)
    return true;
  BLB_Polygon2D **new_lods = realloc(object->lod_polygons, target * sizeof(*new_lods));

  if (!new_lods)
    return false;

  for (size_t i = object->lod_polygon_count; i < target; ++i)
    new_lods[i] = NULL;
  object->lod_polygons = new_lods;
  object->lod_polygon_count = target;

  return true;
}

static BLB_ObjectLODCacheEntry *find_cached_3d(const BLB_Polygon3D *base, int level) {
  if (!base || level <= 0 || !BLB_OBJECT_LOD_CACHE)
    return NULL;
  uint64_t geometry_hash = polygon_geometry_hash3d(base);
  uint32_t error_bits = float_bits(BLB_OBJECT_LOD_ERROR);
  uint32_t reduction_bits = float_bits(BLB_OBJECT_LOD_REDUCTION);

  return cache_find(false, geometry_hash, level, error_bits, reduction_bits, make_key(false, geometry_hash, level, error_bits, reduction_bits));
}

static BLB_ObjectLODCacheEntry *find_cached_2d(const BLB_Polygon2D *base, int level) {
  if (!base || level <= 0 || !BLB_OBJECT_LOD_CACHE)
    return NULL;
  uint64_t geometry_hash = polygon_geometry_hash2d(base);
  uint32_t error_bits = float_bits(BLB_OBJECT_LOD_ERROR);
  uint32_t reduction_bits = float_bits(BLB_OBJECT_LOD_REDUCTION);

  return cache_find(true, geometry_hash, level, error_bits, reduction_bits, make_key(true, geometry_hash, level, error_bits, reduction_bits));
}

bool BLB_Object3D_EnsureLODLevel(BLB_Object3D *object, int level) {
  if (!object || !valid_level_request(level))
    return false;

  if (level == 0 || !lod_eligible_3d(object->polygon))
    return true;

  if (!object->polygon)
    return false;

  if (BLB_OBJECT_LOD_CACHE) {
    object->lod_shared_cache = true;
    int first_missing = 0;

    for (int current = 1; current <= level; ++current) {
      if (!find_cached_3d(object->polygon, current)) {
        first_missing = current;
        break;
      }
    }

    if (first_missing == 0)
      return true;

    const BLB_Polygon3D *source = object->polygon;

    if (first_missing > 1) {
      BLB_ObjectLODCacheEntry *previous = find_cached_3d(object->polygon, first_missing - 1);

      if (previous)
        source = previous->polygon.polygon3d;
    }

    schedule_async_lod3d(object->polygon, source, first_missing, level);

    return false;
  }

  if (!ensure_array3d(object, level))
    return false;

  object->lod_shared_cache = false;

  for (int current = 0; current < level; ++current) {
    if (object->lod_polygons[current])
      continue;
    const BLB_Polygon3D *source = current == 0 ? object->polygon : object->lod_polygons[current - 1];
    object->lod_polygons[current] = generate_lod3d(source);

    if (!object->lod_polygons[current])
      return false;
  }

  return true;
}

bool BLB_Object2D_EnsureLODLevel(BLB_Object2D *object, int level) {
  if (!object || !valid_level_request(level))
    return false;

  if (level == 0)
    return true;

  if (!object->polygon)
    return false;

  if (BLB_OBJECT_LOD_CACHE) {
    object->lod_shared_cache = true;

    for (int i = 0; i < level; ++i) {
      BLB_ObjectLODCacheEntry *entry = find_cached_2d(object->polygon, i + 1);

      if (entry)
        continue;

      const BLB_Polygon2D *source = i == 0 ? object->polygon : NULL;

      if (i > 0) {
        BLB_ObjectLODCacheEntry *previous = find_cached_2d(object->polygon, i);

        if (previous)
          source = previous->polygon.polygon2d;
      }

      if (!source)
        source = object->polygon;

      schedule_async_lod2d(object->polygon, source, i + 1, level);

      return false;
    }

    return true;
  }

  if (!ensure_array2d(object, level))
    return false;

  object->lod_shared_cache = false;

  for (int i = 0; i < level; ++i) {
    if (object->lod_polygons[i])
      continue;
    const BLB_Polygon2D *source = i == 0 ? object->polygon : object->lod_polygons[i - 1];
    object->lod_polygons[i] = generate_lod2d(source);

    if (!object->lod_polygons[i])
      return false;
  }

  return true;
}

bool BLB_Object3D_SetLODLevel(BLB_Object3D *object, int level) {
  if (!object || !valid_level_request(level))
    return false;
  object->lod_level = level;
  object->lod_override = true;

  if (level > 0)
    BLB_Object3D_EnsureLODLevel(object, level);

  return true;
}

bool BLB_Object2D_SetLODLevel(BLB_Object2D *object, int level) {
  if (!object || !valid_level_request(level))
    return false;
  object->lod_level = level;
  object->lod_override = true;

  if (level > 0)
    BLB_Object2D_EnsureLODLevel(object, level);

  return true;
}

void BLB_Object3D_ResetLODOverride(BLB_Object3D *object) {
  if (!object)
    return;
  object->lod_override = false;
  object->lod_level = 0;
}

void BLB_Object2D_ResetLODOverride(BLB_Object2D *object) {
  if (!object)
    return;
  object->lod_override = false;
  object->lod_level = 0;
}

int BLB_Object3D_ResolveLODLevel(const BLB_Object3D *object, float distance) {
  if (!object || !lod_eligible_3d(object->polygon))
    return 0;

  if (object->camera_attached)
    return BLB_OBJECT_LOD && object->lod_override ? clamp_lod_level(object->lod_level) : 0;

  return resolve_level(object->lod_override, object->lod_level, distance);
}

int BLB_Object2D_ResolveLODLevel(const BLB_Object2D *object, float distance) {
  if (!object)
    return 0;

  if (object->camera_attached) {
    if (!BLB_OBJECT_LOD || !object->lod_override)
      return 0;

    return clamp_lod_level(object->lod_level);
  }

  return resolve_level(object->lod_override, object->lod_level, distance);
}

int BLB_Object3D_GetAvailableLODLevel(const BLB_Object3D *object, int requested_level) {
  if (!object || requested_level <= 0)
    return 0;

  for (int level = requested_level; level > 0; --level)
    if (find_cached_3d(object->polygon, level) ||
        (object->lod_polygons && (size_t)level <= object->lod_polygon_count && object->lod_polygons[level - 1]))
      return level;

  return 0;
}

int BLB_Object2D_GetAvailableLODLevel(const BLB_Object2D *object, int requested_level) {
  if (!object || requested_level <= 0)
    return 0;

  for (int level = requested_level; level > 0; --level)
    if (find_cached_2d(object->polygon, level) ||
        (object->lod_polygons && (size_t)level <= object->lod_polygon_count && object->lod_polygons[level - 1]))
      return level;

  return 0;
}

int BLB_Object3D_GetLODLevel(const BLB_Object3D *object) { return object ? object->lod_level : 0; }
int BLB_Object2D_GetLODLevel(const BLB_Object2D *object) { return object ? object->lod_level : 0; }

const BLB_Polygon3D *BLB_Object3D_GetLODPolygonAtLevel(const BLB_Object3D *object, int level) {
  if (!object || level <= 0)
    return object ? object->polygon : NULL;

  if (object->lod_polygons && object->lod_polygon_count > 0) {
    size_t requested = (size_t)level;
    size_t available = requested < object->lod_polygon_count ? requested : object->lod_polygon_count;

    for (size_t i = available; i > 0; --i)
      if (object->lod_polygons[i - 1])
        return object->lod_polygons[i - 1];
  }

  if (BLB_OBJECT_LOD_CACHE && object->polygon) {
    int requested = level;

    while (requested > 0) {
      BLB_ObjectLODCacheEntry *entry = find_cached_3d(object->polygon, requested);

      if (entry)
        return entry->polygon.polygon3d;
      --requested;
    }
  }

  return object->polygon;
}

const BLB_Polygon2D *BLB_Object2D_GetLODPolygonAtLevel(const BLB_Object2D *object, int level) {
  if (!object || level <= 0)
    return object ? object->polygon : NULL;

  if (object->lod_polygons && object->lod_polygon_count > 0) {
    size_t requested = (size_t)level;
    size_t available = requested < object->lod_polygon_count ? requested : object->lod_polygon_count;

    for (size_t i = available; i > 0; --i)
      if (object->lod_polygons[i - 1])
        return object->lod_polygons[i - 1];
  }

  if (BLB_OBJECT_LOD_CACHE && object->polygon) {
    int requested = level;

    while (requested > 0) {
      BLB_ObjectLODCacheEntry *entry = find_cached_2d(object->polygon, requested);

      if (entry)
        return entry->polygon.polygon2d;
      --requested;
    }
  }

  return object->polygon;
}

const BLB_Polygon3D *BLB_Object3D_GetLODPolygon(const BLB_Object3D *object) {
  return object ? BLB_Object3D_GetLODPolygonAtLevel(object, clamp_lod_level(object->lod_level)) : NULL;
}

const BLB_Polygon2D *BLB_Object2D_GetLODPolygon(const BLB_Object2D *object) {
  return object ? BLB_Object2D_GetLODPolygonAtLevel(object, clamp_lod_level(object->lod_level)) : NULL;
}

static void cache_release_3d_polygon(const BLB_Polygon3D *lod) {
  if (!lod || !blb_lod_cache.entries)
    return;

  for (size_t i = 0; i < blb_lod_cache.capacity; ++i) {
    BLB_ObjectLODCacheEntry *entry = &blb_lod_cache.entries[i];

    if (entry->occupied && !entry->is_2d && entry->polygon.polygon3d == lod) {
      cache_release(entry);

      return;
    }
  }
}

static void cache_release_2d_polygon(const BLB_Polygon2D *lod) {
  if (!lod || !blb_lod_cache.entries)
    return;

  for (size_t i = 0; i < blb_lod_cache.capacity; ++i) {
    BLB_ObjectLODCacheEntry *entry = &blb_lod_cache.entries[i];

    if (entry->occupied && entry->is_2d && entry->polygon.polygon2d == lod) {
      cache_release(entry);

      return;
    }
  }
}

void BLB_Object3D_ClearLODs(BLB_Object3D *object) {
  if (!object)
    return;

  if (object->lod_polygons) {
    if (object->lod_shared_cache) {
      for (size_t i = 0; i < object->lod_polygon_count; ++i)
        cache_release_3d_polygon(object->lod_polygons[i]);
    } else {
      for (size_t i = 0; i < object->lod_polygon_count; ++i)
        free_polygon3d_owned(object->lod_polygons[i]);
    }
    free(object->lod_polygons);
  }
  object->lod_polygons = NULL;
  object->lod_polygon_count = 0;
  object->lod_level = 0;
  object->lod_override = false;
  object->lod_shared_cache = false;
}

void BLB_Object2D_ClearLODs(BLB_Object2D *object) {
  if (!object)
    return;

  if (object->lod_polygons) {
    if (object->lod_shared_cache) {
      for (size_t i = 0; i < object->lod_polygon_count; ++i)
        cache_release_2d_polygon(object->lod_polygons[i]);
    } else {
      for (size_t i = 0; i < object->lod_polygon_count; ++i)
        free_polygon2d_owned(object->lod_polygons[i]);
    }
    free(object->lod_polygons);
  }
  object->lod_polygons = NULL;
  object->lod_polygon_count = 0;
  object->lod_level = 0;
  object->lod_override = false;
  object->lod_shared_cache = false;
}

void BLB_ObjectLOD_BeginFrame(uint64_t frame_index) {
  BLB_RenderAsync_Poll();
  blb_lod_cache.current_frame = frame_index;

  if (!BLB_OBJECT_LOD_CACHE || !blb_lod_cache.entries)
    return;
  size_t max_entries = BLB_OBJECT_LOD_CACHE_MAX_ENTRIES > 0 ? (size_t)BLB_OBJECT_LOD_CACHE_MAX_ENTRIES : 1u;

  while (blb_lod_cache.count > max_entries && cache_evict_one(false)) {
  }
}

void BLB_ObjectLODClearCache(void) {
  memset(lod_failed_keys, 0, sizeof(lod_failed_keys));
  lod_failed_cursor = 0;

  if (blb_lod_cache.entries) {
    for (size_t i = 0; i < blb_lod_cache.capacity; ++i) {
      BLB_ObjectLODCacheEntry *entry = &blb_lod_cache.entries[i];

      if (entry->occupied && entry->ref_count == 0) {
        cache_destroy_entry(entry);

        if (blb_lod_cache.count > 0)
          --blb_lod_cache.count;
      }
    }
    cache_rehash();
  }
}

void BLB_ObjectLOD_GetCacheStats(size_t *entries, uint64_t *hits, uint64_t *misses, size_t *live_refs) {
  if (entries)
    *entries = blb_lod_cache.count;
  if (hits)
    *hits = blb_lod_cache.hits;
  if (misses)
    *misses = blb_lod_cache.misses;
  if (live_refs)
    *live_refs = blb_lod_cache.live_refs;
}
