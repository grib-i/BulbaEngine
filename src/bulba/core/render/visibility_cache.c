#include "bulba/core/render/visibility_cache.h"

#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/utils/config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLB_VISIBILITY_CACHE_MIN_CAPACITY 64u
#define BLB_VISIBILITY_CACHE_LOAD_NUMERATOR 7u
#define BLB_VISIBILITY_CACHE_LOAD_DENOMINATOR 10u
#define BLB_VISIBILITY_CACHE_FILE_VERSION 3u
#define BLB_VISIBILITY_CACHE_FILE_NAME "visibility.bin"
#define BLB_VISIBILITY_CAMERA_POSITION_EPSILON 0.0001f
#define BLB_VISIBILITY_CAMERA_MATRIX_EPSILON 0.0001f

typedef struct {
  uint64_t object_key;
  uint64_t transform_key;
  uint64_t transform_revision;
  uint64_t camera_revision;
  uint64_t frame_index;
  float distance;
  bool visible;
  bool occupied;
  BLB_VisibilityObjectKind kind;
} BLB_RenderVisibilityCacheEntry;

struct BLB_RenderVisibilityCache {
  BLB_RenderVisibilityCacheEntry *entries;
  size_t capacity;
  size_t count;

  HMM_Vec3 camera_position;
  HMM_Mat4 view_projection;
  float viewport_width;
  float viewport_height;

  uint64_t camera_key;
  uint64_t disk_camera_key;
  uint64_t camera_revision;
  uint64_t frame_index;

  bool camera_initialized;
  bool disk_loaded;
  bool disk_dirty;

  uint64_t hits;
  uint64_t misses;
};

typedef struct {
  char magic[8];
  uint32_t version;
  uint32_t count;
  uint64_t camera_key;
} BLB_VisibilityCacheFileHeader;

typedef struct {
  uint64_t object_key;
  uint64_t transform_key;
  uint64_t transform_revision;
  float distance;
  uint8_t visible;
  uint8_t kind;
  uint8_t reserved[2];
} BLB_VisibilityCacheFileEntry;

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

static uint64_t hash_u32(uint64_t hash, uint32_t value) { return hash_mix64(hash ^ (uint64_t)value); }

static size_t max_entries(void) {
  if (BLB_OBJECT_VISIBILITY_CACHE_MAX_ENTRIES <= 0)
    return BLB_VISIBILITY_CACHE_MIN_CAPACITY;

  return (size_t)BLB_OBJECT_VISIBILITY_CACHE_MAX_ENTRIES;
}

static uint64_t object_key(BLB_VisibilityObjectKind kind, const void *object) {
  if (!object)
    return 0;

  uint64_t entity_id = 0;

  if (kind == BLB_VISIBILITY_OBJECT_3D)
    entity_id = (uint64_t)((const BLB_Object3D *)object)->entity_id;
  else if (kind == BLB_VISIBILITY_OBJECT_2D)
    entity_id = (uint64_t)((const BLB_Object2D *)object)->entity_id;

  uint64_t hash = hash_mix64(UINT64_C(0x9e3779b97f4a7c15) ^ ((uint64_t)kind << 61));
  hash = hash_mix64(hash ^ entity_id);

  if (entity_id != 0)
    return hash;

  return hash_mix64(hash ^ (uint64_t)(uintptr_t)object);
}

static uint64_t transform_key(BLB_VisibilityObjectKind kind, const void *object) {
  if (!object)
    return 0;

  uint64_t hash = hash_mix64(UINT64_C(0x243f6a8885a308d3) ^ ((uint64_t)kind << 59));

  if (kind == BLB_VISIBILITY_OBJECT_3D) {
    const BLB_Object3D *value = object;
    const uint32_t bits[] = {float_bits(value->position.x),    float_bits(value->position.y), float_bits(value->position.z),
                             float_bits(value->scale.x),       float_bits(value->scale.y),    float_bits(value->scale.z),
                             float_bits(value->bounds_radius), (uint32_t)value->lod_level,    value->visible ? 1u : 0u};

    for (size_t i = 0; i < sizeof(bits) / sizeof(bits[0]); ++i)
      hash = hash_u32(hash ^ ((uint64_t)i << 32), bits[i]);

    return hash;
  }

  if (kind == BLB_VISIBILITY_OBJECT_2D) {
    const BLB_Object2D *value = object;
    const uint32_t bits[] = {float_bits(value->position.x), float_bits(value->position.y),    float_bits(value->rotation), float_bits(value->scale.x),
                             float_bits(value->scale.y),    value->camera_attached ? 1u : 0u, value->visible ? 1u : 0u};

    for (size_t i = 0; i < sizeof(bits) / sizeof(bits[0]); ++i)
      hash = hash_u32(hash ^ ((uint64_t)i << 32), bits[i]);

    return hash;
  }

  return hash;
}

static uint64_t camera_key(HMM_Vec3 camera_position, const HMM_Mat4 *view_projection, float viewport_width, float viewport_height) {
  uint64_t hash = hash_mix64(UINT64_C(0x13198a2e03707344));

  hash = hash_u32(hash, float_bits(camera_position.x));
  hash = hash_u32(hash, float_bits(camera_position.y));
  hash = hash_u32(hash, float_bits(camera_position.z));

  if (view_projection) {
    const float *elements = &view_projection->Elements[0][0];

    for (size_t i = 0; i < 16; ++i)
      hash = hash_u32(hash, float_bits(elements[i]));
  }

  hash = hash_u32(hash, float_bits(viewport_width));
  hash = hash_u32(hash, float_bits(viewport_height));
  hash = hash_u32(hash, BLB_OBJECT_CULLING ? 1u : 0u);
  hash = hash_u32(hash, BLB_OBJECT_FRUSTUM_CULLING ? 1u : 0u);
  hash = hash_u32(hash, BLB_OBJECT_DISTANCE_CULLING ? 1u : 0u);
  hash = hash_u32(hash, float_bits(BLB_OBJECT_MAX_RENDER_DISTANCE));
  hash = hash_u32(hash, float_bits(BLB_OBJECT_CULLING_MARGIN));

  return hash;
}

static bool camera_changed(const BLB_RenderVisibilityCache *cache, HMM_Vec3 position, const HMM_Mat4 *view_projection, float viewport_width,
                           float viewport_height) {
  if (!cache || !view_projection)
    return true;

  if (fabsf(position.x - cache->camera_position.x) > BLB_VISIBILITY_CAMERA_POSITION_EPSILON)
    return true;

  if (fabsf(position.y - cache->camera_position.y) > BLB_VISIBILITY_CAMERA_POSITION_EPSILON)
    return true;

  if (fabsf(position.z - cache->camera_position.z) > BLB_VISIBILITY_CAMERA_POSITION_EPSILON)
    return true;

  if (fabsf(viewport_width - cache->viewport_width) > BLB_VISIBILITY_CAMERA_MATRIX_EPSILON)
    return true;

  if (fabsf(viewport_height - cache->viewport_height) > BLB_VISIBILITY_CAMERA_MATRIX_EPSILON)
    return true;

  const float *current = &view_projection->Elements[0][0];
  const float *previous = &cache->view_projection.Elements[0][0];

  for (size_t i = 0; i < 16; ++i) {
    if (fabsf(current[i] - previous[i]) > BLB_VISIBILITY_CAMERA_MATRIX_EPSILON)
      return true;
  }

  uint64_t current_key = camera_key(position, view_projection, viewport_width, viewport_height);

  return current_key != cache->camera_key;
}

static size_t next_power_of_two(size_t value) {
  size_t result = BLB_VISIBILITY_CACHE_MIN_CAPACITY;

  while (result < value) {
    if (result > SIZE_MAX / 2u)
      return 0;

    result <<= 1u;
  }

  return result;
}

static size_t entry_hash(BLB_VisibilityObjectKind kind, uint64_t object_hash) { return (size_t)hash_mix64(object_hash ^ ((uint64_t)kind << 57)); }

static bool ensure_capacity(BLB_RenderVisibilityCache *cache, size_t wanted) {
  if (!cache)
    return false;

  size_t limit = max_entries();

  if (wanted > limit)
    wanted = limit;

  if (wanted == 0)
    wanted = 1;

  size_t required = wanted * BLB_VISIBILITY_CACHE_LOAD_DENOMINATOR / BLB_VISIBILITY_CACHE_LOAD_NUMERATOR + 1u;

  if (required < BLB_VISIBILITY_CACHE_MIN_CAPACITY)
    required = BLB_VISIBILITY_CACHE_MIN_CAPACITY;

  if (required <= cache->capacity)
    return true;

  size_t new_capacity = next_power_of_two(required);

  if (new_capacity == 0)
    return false;

  BLB_RenderVisibilityCacheEntry *new_entries = calloc(new_capacity, sizeof(*new_entries));

  if (!new_entries)
    return false;

  if (cache->entries) {
    size_t mask = new_capacity - 1u;

    for (size_t i = 0; i < cache->capacity; ++i) {
      BLB_RenderVisibilityCacheEntry *entry = &cache->entries[i];

      if (!entry->occupied)
        continue;

      size_t slot = entry_hash(entry->kind, entry->object_key) & mask;

      while (new_entries[slot].occupied)
        slot = (slot + 1u) & mask;

      new_entries[slot] = *entry;
    }
  }

  free(cache->entries);
  cache->entries = new_entries;
  cache->capacity = new_capacity;

  return true;
}

static BLB_RenderVisibilityCacheEntry *find_entry(BLB_RenderVisibilityCache *cache, BLB_VisibilityObjectKind kind, uint64_t object_hash) {
  if (!cache || !cache->entries || cache->capacity == 0)
    return NULL;

  size_t mask = cache->capacity - 1u;
  size_t slot = entry_hash(kind, object_hash) & mask;

  for (size_t i = 0; i < cache->capacity; ++i) {
    BLB_RenderVisibilityCacheEntry *entry = &cache->entries[slot];

    if (!entry->occupied)
      return NULL;

    if (entry->kind == kind && entry->object_key == object_hash)
      return entry;

    slot = (slot + 1u) & mask;
  }

  return NULL;
}

static bool insert_or_update(BLB_RenderVisibilityCache *cache, BLB_VisibilityObjectKind kind, uint64_t object_hash, uint64_t transform_hash,
                             uint64_t transform_revision, uint64_t camera_revision, uint64_t frame_index, bool visible, float distance) {
  if (!cache)
    return false;

  if (!ensure_capacity(cache, cache->count + 1u))
    return false;

  size_t mask = cache->capacity - 1u;
  size_t slot = entry_hash(kind, object_hash) & mask;

  for (size_t i = 0; i < cache->capacity; ++i) {
    BLB_RenderVisibilityCacheEntry *entry = &cache->entries[slot];

    if (!entry->occupied) {
      entry->occupied = true;
      entry->kind = kind;
      entry->object_key = object_hash;
      entry->transform_key = transform_hash;
      entry->transform_revision = transform_revision;
      entry->camera_revision = camera_revision;
      entry->frame_index = frame_index;
      entry->visible = visible;
      entry->distance = distance;
      cache->count++;
      return true;
    }

    if (entry->kind == kind && entry->object_key == object_hash) {
      entry->transform_key = transform_hash;
      entry->transform_revision = transform_revision;
      entry->camera_revision = camera_revision;
      entry->frame_index = frame_index;
      entry->visible = visible;
      entry->distance = distance;
      return true;
    }

    slot = (slot + 1u) & mask;
  }

  return false;
}

static void clear_entries(BLB_RenderVisibilityCache *cache) {
  if (!cache || !cache->entries || cache->capacity == 0)
    return;

  memset(cache->entries, 0, cache->capacity * sizeof(*cache->entries));
  cache->count = 0;
}

static bool evict_oldest(BLB_RenderVisibilityCache *cache) {
  if (!cache || !cache->entries || cache->count == 0)
    return false;

  size_t oldest = SIZE_MAX;
  uint64_t oldest_frame = UINT64_MAX;

  for (size_t i = 0; i < cache->capacity; ++i) {
    if (!cache->entries[i].occupied)
      continue;

    if (cache->entries[i].frame_index < oldest_frame) {
      oldest_frame = cache->entries[i].frame_index;
      oldest = i;
    }
  }

  if (oldest == SIZE_MAX)
    return false;

  size_t old_count = cache->count;
  BLB_RenderVisibilityCacheEntry *backup = calloc(old_count, sizeof(*backup));

  if (!backup)
    return false;

  size_t backup_count = 0;

  for (size_t i = 0; i < cache->capacity; ++i) {
    if (i == oldest || !cache->entries[i].occupied)
      continue;

    backup[backup_count++] = cache->entries[i];
  }

  memset(cache->entries, 0, cache->capacity * sizeof(*cache->entries));
  cache->count = 0;

  for (size_t i = 0; i < backup_count; ++i)
    insert_or_update(cache, backup[i].kind, backup[i].object_key, backup[i].transform_key, backup[i].transform_revision, backup[i].camera_revision,
                     backup[i].frame_index, backup[i].visible, backup[i].distance);

  free(backup);

  return true;
}

static bool build_cache_path(char *path, size_t size) {
  if (!path || size == 0)
    return false;

  if (!BLB_DEFAULT_CACHE_PATH[0])
    BLB_InitConfig();

  if (!BLB_DEFAULT_CACHE_PATH[0])
    return false;

  int written = snprintf(path, size, "%s/%s", BLB_DEFAULT_CACHE_PATH, BLB_VISIBILITY_CACHE_FILE_NAME);

  return written > 0 && (size_t)written < size;
}

static bool load_disk_cache(BLB_RenderVisibilityCache *cache) {
  if (!cache)
    return false;

  char path[BLB_PATH_MAX + 64];

  if (!build_cache_path(path, sizeof(path)))
    return false;

  FILE *file = fopen(path, "rb");

  if (!file)
    return false;

  BLB_VisibilityCacheFileHeader header = {0};

  if (fread(&header, sizeof(header), 1, file) != 1) {
    fclose(file);
    return false;
  }

  if (memcmp(header.magic, "BULBVC03", 8) != 0 || header.version != BLB_VISIBILITY_CACHE_FILE_VERSION) {
    fclose(file);
    return false;
  }

  size_t limit = max_entries();
  size_t count = header.count < limit ? (size_t)header.count : limit;

  if (!ensure_capacity(cache, count)) {
    fclose(file);
    return false;
  }

  for (size_t i = 0; i < count; ++i) {
    BLB_VisibilityCacheFileEntry data = {0};

    if (fread(&data, sizeof(data), 1, file) != 1)
      break;

    if (data.kind > (uint8_t)BLB_VISIBILITY_OBJECT_2D)
      continue;

    insert_or_update(cache, (BLB_VisibilityObjectKind)data.kind, data.object_key, data.transform_key, data.transform_revision, 0, 0,
                     data.visible != 0, data.distance);
  }

  cache->disk_camera_key = header.camera_key;
  cache->disk_loaded = true;
  cache->disk_dirty = false;

  fclose(file);

  return true;
}

bool BLB_RenderVisibilityCache_Flush(BLB_RenderVisibilityCache *cache) {
  if (!cache || !BLB_OBJECT_VISIBILITY_CACHE)
    return false;

  if (!cache->disk_dirty)
    return true;

  char path[BLB_PATH_MAX + 64];
  char temp_path[BLB_PATH_MAX + 80];

  if (!build_cache_path(path, sizeof(path)))
    return false;

  int written = snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);

  if (written <= 0 || (size_t)written >= sizeof(temp_path))
    return false;

  FILE *file = fopen(temp_path, "wb");

  if (!file)
    return false;

  BLB_VisibilityCacheFileHeader header = {0};
  memcpy(header.magic, "BULBVC03", 8);
  header.version = BLB_VISIBILITY_CACHE_FILE_VERSION;
  header.camera_key = cache->camera_key;

  if (fwrite(&header, sizeof(header), 1, file) != 1) {
    fclose(file);
    remove(temp_path);
    return false;
  }

  uint32_t count = 0;

  for (size_t i = 0; i < cache->capacity; ++i) {
    BLB_RenderVisibilityCacheEntry *entry = &cache->entries[i];

    if (!entry->occupied || entry->camera_revision != cache->camera_revision)
      continue;

    BLB_VisibilityCacheFileEntry data = {0};
    data.object_key = entry->object_key;
    data.transform_key = entry->transform_key;
    data.transform_revision = entry->transform_revision;
    data.distance = entry->distance;
    data.visible = entry->visible ? 1u : 0u;
    data.kind = (uint8_t)entry->kind;

    if (fwrite(&data, sizeof(data), 1, file) != 1) {
      fclose(file);
      remove(temp_path);
      return false;
    }

    ++count;

    if (count == UINT32_MAX)
      break;
  }

  header.count = count;

  if (fseek(file, 0, SEEK_SET) != 0 || fwrite(&header, sizeof(header), 1, file) != 1) {
    fclose(file);
    remove(temp_path);
    return false;
  }

  if (fclose(file) != 0) {
    remove(temp_path);
    return false;
  }

  remove(path);

  if (rename(temp_path, path) != 0) {
    remove(temp_path);
    return false;
  }

  cache->disk_dirty = false;
  cache->disk_camera_key = cache->camera_key;

  return true;
}

BLB_RenderVisibilityCache *BLB_RenderVisibilityCache_Create(void) {
  if (!BLB_DEFAULT_CACHE_PATH[0])
    BLB_InitConfig();

  BLB_RenderVisibilityCache *cache = calloc(1, sizeof(*cache));

  if (!cache)
    return NULL;

  cache->camera_revision = 1;
  load_disk_cache(cache);

  return cache;
}

void BLB_RenderVisibilityCache_Destroy(BLB_RenderVisibilityCache *cache) {
  if (!cache)
    return;

  BLB_RenderVisibilityCache_Flush(cache);

  free(cache->entries);
  free(cache);
}

void BLB_RenderVisibilityCache_BeginFrame(BLB_RenderVisibilityCache *cache, HMM_Vec3 camera_position, const HMM_Mat4 *view_projection,
                                          float viewport_width, float viewport_height) {
  if (!cache || !view_projection || !BLB_OBJECT_VISIBILITY_CACHE)
    return;

  ++cache->frame_index;

  uint64_t current_camera_key = camera_key(camera_position, view_projection, viewport_width, viewport_height);

  if (!cache->camera_initialized) {
    if (cache->disk_loaded && cache->disk_camera_key == current_camera_key) {
      for (size_t i = 0; i < cache->capacity; ++i) {
        if (!cache->entries[i].occupied)
          continue;

        cache->entries[i].camera_revision = cache->camera_revision;
        cache->entries[i].frame_index = cache->frame_index;
      }
    } else {
      clear_entries(cache);
    }

    cache->camera_position = camera_position;
    cache->view_projection = *view_projection;
    cache->viewport_width = viewport_width;
    cache->viewport_height = viewport_height;
    cache->camera_key = current_camera_key;
    cache->camera_initialized = true;
    cache->disk_loaded = false;
    return;
  }

  if (camera_changed(cache, camera_position, view_projection, viewport_width, viewport_height)) {
    ++cache->camera_revision;

    if (cache->camera_revision == 0)
      cache->camera_revision = 1;
  }

  cache->camera_position = camera_position;
  cache->view_projection = *view_projection;
  cache->viewport_width = viewport_width;
  cache->viewport_height = viewport_height;
  cache->camera_key = current_camera_key;
}

void BLB_RenderVisibilityCache_Reserve(BLB_RenderVisibilityCache *cache, size_t object_count) {
  if (!cache || !BLB_OBJECT_VISIBILITY_CACHE)
    return;

  ensure_capacity(cache, object_count);
}

void BLB_RenderVisibilityCache_Clear(BLB_RenderVisibilityCache *cache) {
  if (!cache)
    return;

  clear_entries(cache);
  cache->disk_dirty = true;
}

bool BLB_RenderVisibilityCache_Get(BLB_RenderVisibilityCache *cache, BLB_VisibilityObjectKind kind, const void *object, uint64_t transform_revision,
                                   bool *visible, float *distance) {
  if (!cache || !BLB_OBJECT_VISIBILITY_CACHE || !object || !cache->camera_initialized)
    return false;

  uint64_t object_hash = object_key(kind, object);
  uint64_t current_transform_key = transform_key(kind, object);

  BLB_RenderVisibilityCacheEntry *entry = find_entry(cache, kind, object_hash);

  if (!entry) {
    ++cache->misses;
    return false;
  }

  if (entry->camera_revision != cache->camera_revision || entry->transform_key != current_transform_key) {
    ++cache->misses;
    return false;
  }

  if (transform_revision != 0 && entry->transform_revision != transform_revision) {
    ++cache->misses;
    return false;
  }

  uint64_t age = cache->frame_index >= entry->frame_index ? cache->frame_index - entry->frame_index : UINT64_MAX;

  int max_age = BLB_OBJECT_VISIBILITY_CACHE_MAX_AGE;

  if (BLB_OBJECT_VISIBILITY_CACHE_FAR_DISTANCE > 0.0f && entry->distance >= BLB_OBJECT_VISIBILITY_CACHE_FAR_DISTANCE)
    max_age = BLB_OBJECT_VISIBILITY_CACHE_FAR_MAX_AGE;

  if (max_age < 0)
    max_age = 0;

  if (age > (uint64_t)max_age) {
    ++cache->misses;
    return false;
  }

  ++cache->hits;
  entry->frame_index = cache->frame_index;

  if (visible)
    *visible = entry->visible;

  if (distance)
    *distance = entry->distance;

  return true;
}

void BLB_RenderVisibilityCache_Put(BLB_RenderVisibilityCache *cache, BLB_VisibilityObjectKind kind, const void *object, uint64_t transform_revision,
                                   bool visible, float distance) {
  if (!cache || !BLB_OBJECT_VISIBILITY_CACHE || !object || !cache->camera_initialized)
    return;

  uint64_t object_hash = object_key(kind, object);
  uint64_t current_transform_key = transform_key(kind, object);

  BLB_RenderVisibilityCacheEntry *entry = find_entry(cache, kind, object_hash);

  if (!entry && cache->count >= max_entries())
    evict_oldest(cache);

  if (insert_or_update(cache, kind, object_hash, current_transform_key, transform_revision, cache->camera_revision, cache->frame_index, visible,
                       distance))
    cache->disk_dirty = true;
}

void BLB_RenderVisibilityCache_GetStats(const BLB_RenderVisibilityCache *cache, uint64_t *hits, uint64_t *misses, size_t *entries) {
  if (!cache)
    return;

  if (hits)
    *hits = cache->hits;

  if (misses)
    *misses = cache->misses;

  if (entries)
    *entries = cache->count;
}

uint64_t BLB_RenderVisibilityCache_GetCameraRevision(const BLB_RenderVisibilityCache *cache) { return cache ? cache->camera_revision : 0; }
