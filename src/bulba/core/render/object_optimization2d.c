#include "bulba/core/render/object_optimization2d.h"

#include "bulba/core/render/object_lod.h"
#include "bulba/core/utils/config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static BLB_RenderMode object_render_mode(const BLB_Object2D *object) {
  if (!object)
    return BLB_RENDER_OPAQUE;
  return object->material ? object->material->render_mode : object->render_mode;
}

static const BLB_Polygon2D *active_polygon(const BLB_Object2D *object) { return object ? BLB_Object2D_GetLODPolygon(object) : NULL; }

static uint64_t mix64(uint64_t value) {
  value ^= value >> 30u;
  value *= UINT64_C(0xbf58476d1ce4e5b9);
  value ^= value >> 27u;
  value *= UINT64_C(0x94d049bb133111eb);
  value ^= value >> 31u;
  return value;
}

static void normalize_material_key(BLB_Material *material) {
  if (!material)
    return;
  material->ref_count = 0;
  material->revision = 0;
  material->backend_data = NULL;
  material->backend_destroy = NULL;
}

static uint64_t material_signature(const BLB_Material *material) {
  if (!material)
    return 0;

  BLB_Material key;
  memcpy(&key, material, sizeof(key));
  normalize_material_key(&key);
  const unsigned char *bytes = (const unsigned char *)&key;
  uint64_t hash = UINT64_C(0x510e527fade682d1);

  for (size_t offset = 0; offset < sizeof(key); offset += sizeof(uint64_t)) {
    uint64_t chunk = 0;
    size_t count = sizeof(key) - offset;
    if (count > sizeof(chunk))
      count = sizeof(chunk);
    memcpy(&chunk, bytes + offset, count);
    hash = mix64(hash ^ mix64(chunk + (uint64_t)offset));
  }

  return mix64(hash ^ (uint64_t)sizeof(key));
}

static bool materials_equivalent(const BLB_Material *a, const BLB_Material *b) {
  if (a == b)
    return true;
  if (!a || !b)
    return false;

  BLB_Material key_a;
  BLB_Material key_b;
  memcpy(&key_a, a, sizeof(key_a));
  memcpy(&key_b, b, sizeof(key_b));
  normalize_material_key(&key_a);
  normalize_material_key(&key_b);
  return memcmp(&key_a, &key_b, sizeof(key_a)) == 0;
}

static uint64_t object_signature(const BLB_Object2D *object) {
  if (!object)
    return UINT64_C(0x9e3779b97f4a7c15);

  union {
    float value;
    uint32_t bits;
  } glow = {object->glow}, emission = {object->emission};

  uint64_t h = UINT64_C(0x6a09e667f3bcc909);
  h ^= mix64((uint64_t)(uintptr_t)object);
  h ^= mix64((uint64_t)(uintptr_t)object->polygon);
  h ^= mix64((uint64_t)(uintptr_t)active_polygon(object));
  h ^= mix64((uint64_t)(uintptr_t)object->material);
  h ^= mix64((uint64_t)(uintptr_t)object->texture);
  h ^= mix64((uint64_t)(uintptr_t)(object->material ? object->material->shader_program : NULL));
  h ^= mix64(object->material ? object->material->revision : UINT64_C(0));
  h ^= mix64((uint64_t)object->layer * UINT64_C(0x9e3779b185ebca87));
  h ^= mix64((uint64_t)(uint32_t)object_render_mode(object));
  h ^= mix64(object->camera_attached ? UINT64_C(1) : UINT64_C(0));
  h ^= mix64((uint64_t)(uint32_t)object->lod_level);
  h ^= mix64(object->animation && object->animation->enable ? UINT64_C(1) : UINT64_C(0));
  h ^= mix64((uint64_t)glow.bits << 32u | emission.bits);
  if (object->material) {
    union {
      float value;
      uint32_t bits;
    } material_emission = {object->material->emission_strength}, material_glow = {object->material->glow_strength};
    h ^= mix64((uint64_t)material_emission.bits << 32u | material_glow.bits);
    h ^= mix64((uint64_t)(uint32_t)object->material->render_mode);
  }
  return mix64(h);
}

static uint64_t signature(BLB_Object2D **objects, size_t count) {
  uint64_t sum = UINT64_C(0x243f6a8885a308d3);
  uint64_t xor_hash = UINT64_C(0x13198a2e03707344);
  for (size_t i = 0; i < count; ++i) {
    const uint64_t h = object_signature(objects ? objects[i] : NULL);
    sum += h;
    xor_hash ^= mix64(h + UINT64_C(0x9e3779b97f4a7c15));
  }
  return mix64(sum ^ ((xor_hash << 17u) | (xor_hash >> 47u)) ^ (uint64_t)count * UINT64_C(0xd6e8feb86659fd93));
}

static bool can_instance(const BLB_Object2D *object) {
  if (!object)
    return false;
  const BLB_Polygon2D *polygon = active_polygon(object);
  if (!polygon || !object->material)
    return false;
  if (object_render_mode(object) != BLB_RENDER_OPAQUE || object->material->alpha_mode == BLB_ALPHA_BLEND)
    return false;
  if (object->material->shader_program)
    return false;
  if (object->animation && object->animation->enable)
    return false;
  if (object->glow > 0.00001f || object->emission > 0.00001f || object->material->emission_strength > 0.00001f ||
      object->material->glow_strength > 0.00001f)
    return false;
  return polygon->vertex_count > 0 && polygon->indices && polygon->index_count >= 3;
}

static bool key_equal(const BLB_RenderInstances2DGroup *group, const BLB_Object2D *object) {
  if (!group || !object)
    return false;
  const BLB_Polygon2D *polygon = active_polygon(object);
  return group->polygon == polygon && materials_equivalent(group->material, object->material) && group->texture == object->texture &&
         group->layer == object->layer && group->render_mode == object_render_mode(object) && group->camera_attached == object->camera_attached;
}

static uint64_t group_key_hash(const BLB_Polygon2D *polygon, const BLB_Material *material, const BLB_Texture *texture, unsigned short layer,
                               BLB_RenderMode render_mode, bool camera_attached) {
  uint64_t h = mix64((uint64_t)(uintptr_t)polygon);
  h ^= mix64(material_signature(material) + UINT64_C(0x9e3779b97f4a7c15));
  h ^= mix64((uint64_t)(uintptr_t)texture + UINT64_C(0xbf58476d1ce4e5b9));
  h ^= mix64((uint64_t)layer * UINT64_C(0x94d049bb133111eb));
  h ^= mix64((uint64_t)(uint32_t)render_mode);
  h ^= mix64(camera_attached ? UINT64_C(1) : UINT64_C(0));
  return mix64(h);
}

static uint64_t object_group_hash(const BLB_Object2D *object) {
  return object ? group_key_hash(active_polygon(object), object->material, object->texture, object->layer, object_render_mode(object), object->camera_attached) : 0;
}

static uint64_t group_hash(const BLB_RenderInstances2DGroup *group) {
  return group ? group_key_hash(group->polygon, group->material, group->texture, group->layer, group->render_mode, group->camera_attached) : 0;
}

static BLB_RenderInstances2DGroup *find_group_linear(const BLB_ObjectOptimization2D *optimization, const BLB_Object2D *object) {
  if (!optimization || !object)
    return NULL;
  for (size_t i = 0; i < optimization->group_count; ++i)
    if (key_equal(optimization->groups[i], object))
      return optimization->groups[i];
  return NULL;
}

static bool rebuild_group_slots(BLB_RenderInstances2DGroup ***slots, size_t *capacity, BLB_RenderInstances2DGroup **groups, size_t count,
                                size_t new_capacity) {
  if (!slots || !capacity || !groups || new_capacity < 16u || (new_capacity & (new_capacity - 1u)) != 0)
    return false;
  if (new_capacity > SIZE_MAX / sizeof(**slots))
    return false;
  BLB_RenderInstances2DGroup **replacement = calloc(new_capacity, sizeof(*replacement));
  if (!replacement)
    return false;
  for (size_t i = 0; i < count; ++i) {
    BLB_RenderInstances2DGroup *group = groups[i];
    if (!group)
      continue;
    size_t slot = (size_t)group_hash(group) & (new_capacity - 1u);
    while (replacement[slot])
      slot = (slot + 1u) & (new_capacity - 1u);
    replacement[slot] = group;
  }
  free(*slots);
  *slots = replacement;
  *capacity = new_capacity;
  return true;
}

static BLB_RenderInstances2DGroup *find_group_hashed(BLB_RenderInstances2DGroup **slots, size_t capacity, const BLB_Object2D *object) {
  if (!slots || capacity == 0 || !object)
    return NULL;
  size_t slot = (size_t)object_group_hash(object) & (capacity - 1u);
  while (slots[slot]) {
    if (key_equal(slots[slot], object))
      return slots[slot];
    slot = (slot + 1u) & (capacity - 1u);
  }
  return NULL;
}

static bool insert_group_hashed(BLB_RenderInstances2DGroup **slots, size_t capacity, BLB_RenderInstances2DGroup *group) {
  if (!slots || capacity == 0 || !group)
    return false;
  size_t slot = (size_t)group_hash(group) & (capacity - 1u);
  while (slots[slot])
    slot = (slot + 1u) & (capacity - 1u);
  slots[slot] = group;
  return true;
}

static bool grow_groups(BLB_ObjectOptimization2D *optimization) {
  if (!optimization)
    return false;
  if (optimization->group_count < optimization->group_capacity)
    return true;
  size_t next = optimization->group_capacity ? optimization->group_capacity : 8u;
  if (optimization->group_capacity && next > SIZE_MAX / 2u)
    return false;
  if (optimization->group_capacity)
    next *= 2u;
  if (next < optimization->group_count + 1u)
    next = optimization->group_count + 1u;
  if (next > SIZE_MAX / sizeof(*optimization->groups))
    return false;
  BLB_RenderInstances2DGroup **groups = realloc(optimization->groups, next * sizeof(*groups));
  if (!groups)
    return false;
  optimization->groups = groups;
  optimization->group_capacity = next;
  return true;
}

static bool add_object(BLB_RenderInstances2DGroup *group, BLB_Object2D *object) {
  if (!group || !object)
    return false;
  if (group->object_count >= group->object_capacity) {
    size_t capacity = group->object_capacity ? group->object_capacity : 8u;
    if (group->object_capacity && capacity > SIZE_MAX / 2u)
      return false;
    if (group->object_capacity)
      capacity *= 2u;
    if (capacity < group->object_count + 1u)
      capacity = group->object_count + 1u;
    if (capacity > SIZE_MAX / sizeof(*group->objects))
      return false;
    BLB_Object2D **objects = realloc(group->objects, capacity * sizeof(*objects));
    if (!objects)
      return false;
    group->objects = objects;
    group->object_capacity = capacity;
  }
  group->objects[group->object_count++] = object;
  return true;
}

static bool prepare_batches(BLB_ObjectOptimization2D *optimization) {
  for (size_t i = 0; i < optimization->group_count; ++i) {
    BLB_RenderInstances2DGroup *group = optimization->groups[i];
    if (!group || !group->enabled || group->object_count == 0)
      continue;
    if (group->object_count < (size_t)(BLB_OBJECT_2D_INSTANCING_MIN_OBJECTS > 1 ? BLB_OBJECT_2D_INSTANCING_MIN_OBJECTS : 2)) {
      group->enabled = false;
      continue;
    }
    size_t batch_capacity = BLB_OBJECT_INSTANCING_MAX_INSTANCES > 0 ? (size_t)BLB_OBJECT_INSTANCING_MAX_INSTANCES : 1u;
    if (batch_capacity > group->object_count)
      batch_capacity = group->object_count;
    if (batch_capacity > SIZE_MAX / sizeof(*group->batch)) {
      group->enabled = false;
      continue;
    }
    group->batch = malloc(batch_capacity * sizeof(*group->batch));
    if (!group->batch) {
      group->enabled = false;
      continue;
    }
    group->batch_capacity = batch_capacity;
  }
  return true;
}

BLB_ObjectOptimization2D *BLB_ObjectOptimization2D_Create(BLB_Object2D **objects, size_t count) {
  BLB_ObjectOptimization2D *result = calloc(1, sizeof(*result));
  if (!result)
    return NULL;

  result->source_count = count;
  result->source_signature = signature(objects, count);

  size_t slot_capacity = 16u;
  BLB_RenderInstances2DGroup **slots = calloc(slot_capacity, sizeof(*slots));
  if (!slots)
    slot_capacity = 0;

  for (size_t i = 0; i < count; ++i) {
    BLB_Object2D *object = objects ? objects[i] : NULL;
    if (!can_instance(object))
      continue;

    if (slot_capacity && result->group_count >= slot_capacity - slot_capacity / 3u) {
      const bool resized = slot_capacity <= SIZE_MAX / 2u &&
                           rebuild_group_slots(&slots, &slot_capacity, result->groups, result->group_count, slot_capacity * 2u);
      if (!resized) {
        free(slots);
        slots = NULL;
        slot_capacity = 0;
      }
    }

    BLB_RenderInstances2DGroup *group = slot_capacity ? find_group_hashed(slots, slot_capacity, object) : find_group_linear(result, object);
    if (!group) {
      if (!grow_groups(result))
        continue;
      group = calloc(1, sizeof(*group));
      if (!group)
        continue;
      group->polygon = active_polygon(object);
      group->material = object->material;
      group->texture = object->texture;
      group->shader = object->material ? object->material->shader_program : NULL;
      group->layer = object->layer;
      group->render_mode = object_render_mode(object);
      memcpy(group->color, &object->color, sizeof(group->color));
      group->camera_attached = object->camera_attached;
      group->enabled = true;
      result->groups[result->group_count++] = group;
      if (slot_capacity && !insert_group_hashed(slots, slot_capacity, group)) {
        free(slots);
        slots = NULL;
        slot_capacity = 0;
      }
    }

    if (!add_object(group, object)) {
      group->enabled = false;
      continue;
    }
    object->optimization_instance_group = group;
  }

  free(slots);
  prepare_batches(result);
  return result;
}

void BLB_ObjectOptimization2D_Destroy(BLB_ObjectOptimization2D *optimization) {
  if (!optimization)
    return;
  for (size_t i = 0; i < optimization->group_count; ++i) {
    BLB_RenderInstances2DGroup *group = optimization->groups[i];
    if (!group)
      continue;
    for (size_t j = 0; j < group->object_count; ++j) {
      BLB_Object2D *object = group->objects[j];
      if (object && object->optimization_instance_group == group)
        object->optimization_instance_group = NULL;
    }
    free(group->batch);
    free(group->objects);
    free(group);
  }
  free(optimization->groups);
  free(optimization);
}

bool BLB_ObjectOptimization2D_MatchesSource(const BLB_ObjectOptimization2D *optimization, BLB_Object2D **objects, size_t count) {
  if (!optimization || optimization->source_count != count)
    return false;
  return optimization->source_signature == signature(objects, count);
}

BLB_RenderInstances2DGroup *BLB_ObjectOptimization2D_FindGroup(const BLB_ObjectOptimization2D *optimization, const BLB_Object2D *object) {
  (void)optimization;
  if (!object)
    return NULL;
  return (BLB_RenderInstances2DGroup *)object->optimization_instance_group;
}

void BLB_ObjectOptimization2D_ResetDrawState(BLB_ObjectOptimization2D *optimization) {
  if (!optimization)
    return;
  for (size_t i = 0; i < optimization->group_count; ++i)
    if (optimization->groups[i])
      optimization->groups[i]->rendered = false;
}
