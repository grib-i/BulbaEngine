#include "object_optimization2d.h"

#include "bulba/core/render/object_lod.h"
#include "bulba/core/utils/config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static BLB_RenderMode object_render_mode(const BLB_Object2D *object) {
  if (!object)
    return BLB_RENDER_OPAQUE;

  if (object->material) {
    BLB_RenderMode mode = object->material->render_mode;

    return mode == BLB_RENDER_OPAQUE && object->material->alpha_mode == BLB_ALPHA_BLEND ? BLB_RENDER_TRANSPARENT : mode;
  }

  return object->render_mode;
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
  material->base_color = BLB_COLOR_TRANSPARENT;
  memset(material->name, 0, sizeof(material->name));
  material->texture_retain = NULL;
  material->texture_release = NULL;
  material->texture_user_data = NULL;
  material->user_data = NULL;
  material->ref_count = 0;
  material->revision = 0;
  material->backend_data = NULL;
  material->backend_destroy = NULL;
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
  uint64_t hash = mix64((uint64_t)count);

  for (size_t i = 0; i < count; ++i)
    hash = mix64(hash ^ object_signature(objects ? objects[i] : NULL));

  return hash;
}

static bool can_instance(const BLB_Object2D *object) {
  if (!object)
    return false;
  const BLB_Polygon2D *polygon = active_polygon(object);

  if (!polygon || !object->material)
    return false;

  if (object->material->shader_program)
    return false;

  if (object->material->glow_strength > 0.00001f)
    return false;

  return polygon->vertex_count > 0 && polygon->indices && polygon->index_count >= 3;
}

static bool key_equal(const BLB_RenderInstances2DGroup *group, const BLB_Object2D *object) {
  if (!group || !object)
    return false;
  const BLB_Polygon2D *polygon = active_polygon(object);

  return group->polygon == polygon && materials_equivalent(group->material, object->material) && group->layer == object->layer &&
         group->render_mode == object_render_mode(object) && group->camera_attached == object->camera_attached;
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

    if (group->object_count < 2u) {
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

  BLB_RenderInstances2DGroup *previous = NULL;

  for (size_t i = 0; i < count; ++i) {
    BLB_Object2D *object = objects ? objects[i] : NULL;

    if (!can_instance(object)) {
      previous = NULL;
      continue;
    }
    BLB_RenderInstances2DGroup *group = previous && key_equal(previous, object) ? previous : NULL;

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
      group->color = object->color;
      group->camera_attached = object->camera_attached;
      group->enabled = true;
      result->groups[result->group_count++] = group;
    }

    if (!add_object(group, object)) {
      group->enabled = false;
      continue;
    }
    object->optimization_instance_group = group;
    previous = group;
  }

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
