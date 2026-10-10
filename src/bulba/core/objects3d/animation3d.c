#include "bulba/core/objects3d/animation3d.h"
#include "bulba/core/objects3d/model_loader_common.h"
#include "model_animation_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { BLB_ANIMATION_OVERRIDE_POSITION = 1u << 0, BLB_ANIMATION_OVERRIDE_ROTATION = 1u << 1, BLB_ANIMATION_OVERRIDE_SCALE = 1u << 2 };

static BLB_Polygon3D *clone_polygon(const BLB_Polygon3D *source) {
  if (!source || !source->vertices || !source->indices || !source->vertex_count || !source->index_count)
    return NULL;
  BLB_Polygon3D *copy = calloc(1, sizeof(*copy));
  if (!copy)
    return NULL;
  copy->vertices = malloc(source->vertex_count * sizeof(*copy->vertices));
  copy->base_vertices = malloc(source->vertex_count * sizeof(*copy->base_vertices));
  copy->normals = source->normals ? malloc(source->vertex_count * sizeof(*copy->normals)) : NULL;
  copy->uvs = source->uvs ? malloc(source->vertex_count * sizeof(*copy->uvs)) : NULL;
  copy->indices = malloc(source->index_count * sizeof(*copy->indices));
  if (!copy->vertices || !copy->base_vertices || (source->normals && !copy->normals) || (source->uvs && !copy->uvs) || !copy->indices) {
    BLB_ModelFreePolygon(copy);
    return NULL;
  }
  memcpy(copy->vertices, source->vertices, source->vertex_count * sizeof(*copy->vertices));
  if (source->base_vertices)
    memcpy(copy->base_vertices, source->base_vertices, source->vertex_count * sizeof(*copy->base_vertices));
  else
    memcpy(copy->base_vertices, source->vertices, source->vertex_count * sizeof(*copy->base_vertices));
  if (source->normals)
    memcpy(copy->normals, source->normals, source->vertex_count * sizeof(*copy->normals));
  if (source->uvs)
    memcpy(copy->uvs, source->uvs, source->vertex_count * sizeof(*copy->uvs));
  memcpy(copy->indices, source->indices, source->index_count * sizeof(*copy->indices));
  copy->vertex_count = source->vertex_count;
  copy->index_count = source->index_count;
  copy->normal = source->normal;
  copy->geometry_hash = source->geometry_hash;
  copy->dynamic_vertices = true;
  return copy;
}

void BLB_ModelAnimationAsset_Destroy(BLB_ModelAnimationAsset *asset) {
  if (!asset)
    return;
  if (asset->nodes) {
    for (size_t i = 0; i < asset->node_count; ++i)
      free(asset->nodes[i].name);
  }

  if (asset->clips) {
    for (size_t i = 0; i < asset->clip_count; ++i) {
      BLB_ModelAnimationClip *clip = &asset->clips[i];
      free(clip->name);
      if (clip->channels) {
        for (size_t j = 0; j < clip->channel_count; ++j) {
          free(clip->channels[j].positions);
          free(clip->channels[j].rotations);
          free(clip->channels[j].scales);
        }
      }

      free(clip->channels);
    }
  }

  if (asset->skins) {
    for (size_t i = 0; i < asset->skin_count; ++i) {
      free(asset->skins[i].vertices);
      free(asset->skins[i].offsets);
    }
  }

  free(asset->skins);
  free(asset->clips);
  free(asset->nodes);
  free(asset);
}

static HMM_Mat4 node_local_matrix(HMM_Vec3 position, HMM_Quat rotation, HMM_Vec3 scale) {
  return HMM_MulM4(HMM_Translate(position), HMM_MulM4(HMM_QToM4(rotation), HMM_Scale(scale)));
}

static HMM_Vec3 sample_vector_keys(const BLB_ModelVectorKey *keys, size_t count, double time, HMM_Vec3 fallback) {
  if (!keys || count == 0)
    return fallback;
  if (time <= keys[0].time)
    return keys[0].value;
  if (time >= keys[count - 1].time)
    return keys[count - 1].value;
  for (size_t i = 1; i < count; ++i) {
    if (time <= keys[i].time) {
      double span = keys[i].time - keys[i - 1].time;
      float t = span > 0.0 ? (float)((time - keys[i - 1].time) / span) : 0.0f;
      return HMM_LerpV3(keys[i - 1].value, t, keys[i].value);
    }
  }
  return keys[count - 1].value;
}

static HMM_Quat sample_quaternion_keys(const BLB_ModelQuaternionKey *keys, size_t count, double time, HMM_Quat fallback) {
  if (!keys || count == 0)
    return fallback;
  if (time <= keys[0].time)
    return keys[0].value;
  if (time >= keys[count - 1].time)
    return keys[count - 1].value;
  for (size_t i = 1; i < count; ++i) {
    if (time <= keys[i].time) {
      double span = keys[i].time - keys[i - 1].time;
      float t = span > 0.0 ? (float)((time - keys[i - 1].time) / span) : 0.0f;
      return HMM_SLerp(keys[i - 1].value, t, keys[i].value);
    }
  }
  return keys[count - 1].value;
}

static HMM_Vec3 transform_position(HMM_Mat4 matrix, HMM_Vec3 position) {
  HMM_Vec4 result = HMM_MulM4V4(matrix, HMM_V4(position.x, position.y, position.z, 1.0f));
  if (fabsf(result.w) > 0.000001f && fabsf(result.w - 1.0f) > 0.000001f)
    return HMM_V3(result.x / result.w, result.y / result.w, result.z / result.w);
  return HMM_V3(result.x, result.y, result.z);
}

static HMM_Vec3 transform_direction(HMM_Mat4 matrix, HMM_Vec3 direction) {
  HMM_Vec4 result = HMM_MulM4V4(matrix, HMM_V4(direction.x, direction.y, direction.z, 0.0f));
  HMM_Vec3 normal = HMM_V3(result.x, result.y, result.z);
  return HMM_LenV3(normal) > 0.000001f ? HMM_NormV3(normal) : HMM_V3(0.0f, 1.0f, 0.0f);
}

static float polygon_bounds_diagonal(const HMM_Vec3 *vertices, size_t count, bool *valid) {
  if (valid)
    *valid = false;
  if (!vertices || count == 0)
    return 0.0f;

  HMM_Vec3 min_v = HMM_V3(FLT_MAX, FLT_MAX, FLT_MAX);
  HMM_Vec3 max_v = HMM_V3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (size_t i = 0; i < count; ++i) {
    HMM_Vec3 v = vertices[i];
    if (!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.z))
      return INFINITY;
    min_v.x = fminf(min_v.x, v.x);
    min_v.y = fminf(min_v.y, v.y);
    min_v.z = fminf(min_v.z, v.z);
    max_v.x = fmaxf(max_v.x, v.x);
    max_v.y = fmaxf(max_v.y, v.y);
    max_v.z = fmaxf(max_v.z, v.z);
  }
  if (valid)
    *valid = true;
  return HMM_LenV3(HMM_SubV3(max_v, min_v));
}

static void skin_instance(BLB_Object3D *object, BLB_ModelAnimationInstance *instance) {
  BLB_ModelAnimationAsset *asset = instance->asset;
  if (!object || !asset)
    return;
  for (size_t i = 0; i < asset->node_count; ++i) {
    HMM_Mat4 local = asset->nodes[i].bind_local;
    if (instance->transform_dirty && instance->transform_dirty[i]) {
      HMM_Mat4 animated_trs = node_local_matrix(instance->positions[i], instance->rotations[i], instance->scales[i]);
      local = HMM_MulM4(animated_trs, asset->nodes[i].bind_residual);
    }
    int parent = asset->nodes[i].parent_index;
    instance->global_transforms[i] = parent >= 0 ? HMM_MulM4(instance->global_transforms[parent], local) : local;
  }
  const size_t count = object->submesh_count < asset->skin_count ? object->submesh_count : asset->skin_count;
  for (size_t mesh_index = 0; mesh_index < count; ++mesh_index) {
    BLB_Polygon3D *polygon = object->submeshes[mesh_index].polygon;
    BLB_ModelSkinSubmesh *skin = &asset->skins[mesh_index];
    if (!polygon || !skin->vertices || skin->vertex_count != polygon->vertex_count)
      continue;

    for (size_t vertex_index = 0; vertex_index < polygon->vertex_count; ++vertex_index) {
      const BLB_ModelSkinVertex *influences = &skin->vertices[vertex_index];
      HMM_Vec3 source_position = polygon->base_vertices[vertex_index];
      HMM_Vec3 source_normal = instance->source_normals && instance->source_normals[mesh_index]
                                   ? instance->source_normals[mesh_index][vertex_index]
                                   : (polygon->normals ? polygon->normals[vertex_index] : HMM_V3(0.0f, 1.0f, 0.0f));

      HMM_Vec3 position = HMM_V3(0.0f, 0.0f, 0.0f);
      HMM_Vec3 normal = HMM_V3(0.0f, 0.0f, 0.0f);
      float total_weight = 0.0f;

      for (size_t influence = 0; influence < 4; ++influence) {
        float weight = influences->weights[influence];
        uint32_t node_index = influences->node_indices[influence];

        if (weight <= 0.0f || node_index >= asset->node_count)
          continue;

        HMM_Mat4 transform = HMM_MulM4(asset->root_inverse, HMM_MulM4(instance->global_transforms[node_index], skin->offsets[node_index]));
        position = HMM_AddV3(position, HMM_MulV3F(transform_position(transform, source_position), weight));
        normal = HMM_AddV3(normal, HMM_MulV3F(transform_direction(transform, source_normal), weight));
        total_weight += weight;
      }
      if (total_weight <= 0.000001f) {
        polygon->vertices[vertex_index] = source_position;
        if (polygon->normals)
          polygon->normals[vertex_index] = source_normal;
      } else {
        polygon->vertices[vertex_index] = HMM_MulV3F(position, 1.0f / total_weight);
        if (polygon->normals)
          polygon->normals[vertex_index] = HMM_LenV3(normal) > 0.000001f ? HMM_NormV3(normal) : source_normal;
      }
    }

    if (instance->bind_pose_captured && instance->bind_pose_vertices && instance->bind_pose_vertices[mesh_index]) {
      bool current_valid = false;
      bool bind_valid = false;
      float current_diagonal = polygon_bounds_diagonal(polygon->vertices, polygon->vertex_count, &current_valid);
      float bind_diagonal = polygon_bounds_diagonal(instance->bind_pose_vertices[mesh_index], polygon->vertex_count, &bind_valid);
      float allowed_diagonal = fmaxf(5.0f, bind_diagonal * 6.0f);

      if (!current_valid || !bind_valid || !isfinite(current_diagonal) || current_diagonal > allowed_diagonal) {
        memcpy(polygon->vertices, instance->bind_pose_vertices[mesh_index], polygon->vertex_count * sizeof(*polygon->vertices));
        if (polygon->normals && instance->bind_pose_normals && instance->bind_pose_normals[mesh_index])
          memcpy(polygon->normals, instance->bind_pose_normals[mesh_index], polygon->vertex_count * sizeof(*polygon->normals));
      }
    }
  }
  if (instance->combined_polygon && object->polygon) {
    size_t offset = 0;
    for (size_t i = 0; i < object->submesh_count; ++i) {
      BLB_Polygon3D *polygon = object->submeshes[i].polygon;
      if (!polygon || offset + polygon->vertex_count > instance->combined_polygon->vertex_count)
        break;
      memcpy(instance->combined_polygon->vertices + offset, polygon->vertices, polygon->vertex_count * sizeof(*polygon->vertices));
      if (instance->combined_polygon->normals && polygon->normals)
        memcpy(instance->combined_polygon->normals + offset, polygon->normals, polygon->vertex_count * sizeof(*polygon->normals));
      offset += polygon->vertex_count;
    }
  }
  object->mesh.vertices = object->polygon ? object->polygon->vertices : NULL;
  object->mesh.normals = object->polygon ? object->polygon->normals : NULL;
}

int BLB_ModelAnimationInstance_Create(BLB_Object3D *object, BLB_ModelAnimationAsset *asset) {
  if (!object || !asset || asset->node_count == 0 || asset->skin_count == 0 || object->submesh_count == 0)
    return -1;

  BLB_ModelAnimationInstance *instance = calloc(1, sizeof(*instance));
  if (!instance)
    return -1;

  instance->asset = asset;
  instance->active_clip = -1;
  instance->speed = 1.0f;
  instance->loop = true;
  size_t nodes = asset->node_count;
  instance->positions = malloc(nodes * sizeof(*instance->positions));
  instance->rotations = malloc(nodes * sizeof(*instance->rotations));
  instance->scales = malloc(nodes * sizeof(*instance->scales));
  instance->manual_positions = malloc(nodes * sizeof(*instance->manual_positions));
  instance->manual_rotations = malloc(nodes * sizeof(*instance->manual_rotations));
  instance->manual_scales = malloc(nodes * sizeof(*instance->manual_scales));
  instance->manual_overrides = calloc(nodes, sizeof(*instance->manual_overrides));
  instance->transform_dirty = calloc(nodes, sizeof(*instance->transform_dirty));
  instance->global_transforms = malloc(nodes * sizeof(*instance->global_transforms));
  instance->submesh_count = object->submesh_count;
  instance->submesh_polygons = calloc(instance->submesh_count, sizeof(*instance->submesh_polygons));
  instance->source_normals = calloc(instance->submesh_count, sizeof(*instance->source_normals));
  instance->bind_pose_vertices = calloc(instance->submesh_count, sizeof(*instance->bind_pose_vertices));
  instance->bind_pose_normals = calloc(instance->submesh_count, sizeof(*instance->bind_pose_normals));

  if (!instance->positions || !instance->rotations || !instance->scales || !instance->manual_positions || !instance->manual_rotations ||
      !instance->manual_scales || !instance->manual_overrides || !instance->transform_dirty || !instance->global_transforms ||
      !instance->submesh_polygons || !instance->source_normals || !instance->bind_pose_vertices || !instance->bind_pose_normals)
    goto fail;

  for (size_t i = 0; i < nodes; ++i) {
    instance->positions[i] = asset->nodes[i].bind_position;
    instance->rotations[i] = asset->nodes[i].bind_rotation;
    instance->scales[i] = asset->nodes[i].bind_scale;
    instance->manual_positions[i] = asset->nodes[i].bind_position;
    instance->manual_rotations[i] = asset->nodes[i].bind_rotation;
    instance->manual_scales[i] = asset->nodes[i].bind_scale;
  }

  for (size_t i = 0; i < object->submesh_count; ++i) {
    instance->submesh_polygons[i] = clone_polygon(object->submeshes[i].polygon);
    if (!instance->submesh_polygons[i])
      goto fail;

    BLB_Polygon3D *polygon = instance->submesh_polygons[i];
    if (polygon->normals && polygon->vertex_count) {
      instance->source_normals[i] = malloc(polygon->vertex_count * sizeof(*instance->source_normals[i]));
      if (!instance->source_normals[i])
        goto fail;
      memcpy(instance->source_normals[i], polygon->normals, polygon->vertex_count * sizeof(*instance->source_normals[i]));
    }
  }

  instance->combined_polygon = clone_polygon(object->polygon);

  if (!instance->combined_polygon)
    goto fail;

  for (size_t i = 0; i < object->submesh_count; ++i)
    object->submeshes[i].polygon = instance->submesh_polygons[i];

  object->polygon = instance->combined_polygon;

  object->geometry_id ^= (uint64_t)(uintptr_t)instance;
  if (!object->geometry_id)
    object->geometry_id = 1;

  object->owns_model_geometry = true;
  object->animation3d = instance;
  object->optimization_dynamic = true;
  object->optimization_stable_frames = 0;
  object->optimization_static_recovered = false;

  skin_instance(object, instance);

  bool bind_pose_ok = true;
  for (size_t i = 0; i < object->submesh_count; ++i) {
    BLB_Polygon3D *polygon = object->submeshes[i].polygon;
    if (!polygon || !polygon->vertices || polygon->vertex_count == 0) {
      bind_pose_ok = false;
      break;
    }

    instance->bind_pose_vertices[i] = malloc(polygon->vertex_count * sizeof(*instance->bind_pose_vertices[i]));
    if (!instance->bind_pose_vertices[i]) {
      bind_pose_ok = false;
      break;
    }

    memcpy(instance->bind_pose_vertices[i], polygon->vertices, polygon->vertex_count * sizeof(*instance->bind_pose_vertices[i]));
    if (polygon->normals) {
      instance->bind_pose_normals[i] = malloc(polygon->vertex_count * sizeof(*instance->bind_pose_normals[i]));
      if (!instance->bind_pose_normals[i]) {
        bind_pose_ok = false;
        break;
      }

      memcpy(instance->bind_pose_normals[i], polygon->normals, polygon->vertex_count * sizeof(*instance->bind_pose_normals[i]));
    }
  }
  instance->bind_pose_captured = bind_pose_ok;
  if (!bind_pose_ok) {
    for (size_t i = 0; i < instance->submesh_count; ++i) {
      free(instance->bind_pose_vertices[i]);
      free(instance->bind_pose_normals[i]);
      instance->bind_pose_vertices[i] = NULL;
      instance->bind_pose_normals[i] = NULL;
    }
  }
  return 0;
fail:
  for (size_t i = 0; i < instance->submesh_count; ++i) {
    BLB_ModelFreePolygon(instance->submesh_polygons ? instance->submesh_polygons[i] : NULL);
    free(instance->source_normals ? instance->source_normals[i] : NULL);
    free(instance->bind_pose_vertices ? instance->bind_pose_vertices[i] : NULL);
    free(instance->bind_pose_normals ? instance->bind_pose_normals[i] : NULL);
  }

  BLB_ModelFreePolygon(instance->combined_polygon);
  free(instance->positions);
  free(instance->rotations);
  free(instance->scales);
  free(instance->manual_positions);
  free(instance->manual_rotations);
  free(instance->manual_scales);
  free(instance->manual_overrides);
  free(instance->transform_dirty);
  free(instance->global_transforms);
  free(instance->submesh_polygons);
  free(instance->source_normals);
  free(instance->bind_pose_vertices);
  free(instance->bind_pose_normals);
  free(instance);
  return -1;
}

void BLB_ModelAnimationInstance_Destroy(BLB_Object3D *object) {
  if (!object || !object->animation3d)
    return;

  BLB_ModelAnimationInstance *instance = object->animation3d;
  free(instance->positions);
  free(instance->rotations);
  free(instance->scales);
  free(instance->manual_positions);
  free(instance->manual_rotations);
  free(instance->manual_scales);
  free(instance->manual_overrides);
  free(instance->transform_dirty);

  for (size_t i = 0; i < instance->submesh_count; ++i) {
    free(instance->source_normals ? instance->source_normals[i] : NULL);
    free(instance->bind_pose_vertices ? instance->bind_pose_vertices[i] : NULL);
    free(instance->bind_pose_normals ? instance->bind_pose_normals[i] : NULL);
  }

  free(instance->global_transforms);
  free(instance->submesh_polygons);
  free(instance->source_normals);
  free(instance->bind_pose_vertices);
  free(instance->bind_pose_normals);
  free(instance);
  object->animation3d = NULL;
}

static size_t find_node(const BLB_ModelAnimationAsset *asset, const char *name) {
  if (!asset || !name)
    return SIZE_MAX;

  for (size_t i = 0; i < asset->node_count; ++i)
    if (asset->nodes[i].is_bone && asset->nodes[i].name && strcmp(asset->nodes[i].name, name) == 0)
      return i;
  return SIZE_MAX;
}

size_t BLB_Model_GetBoneCount(const BLB_Object3D *object) {
  const BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (!instance)
    return 0;

  size_t count = 0;
  for (size_t i = 0; i < instance->asset->node_count; ++i)
    if (instance->asset->nodes[i].is_bone)
      ++count;
  return count;
}

const char *BLB_Model_GetBoneName(const BLB_Object3D *object, size_t index) {
  const BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (!instance)
    return NULL;
  for (size_t i = 0; i < instance->asset->node_count; ++i) {
    if (instance->asset->nodes[i].is_bone) {
      if (index == 0)
        return instance->asset->nodes[i].name;
      --index;
    }
  }
  return NULL;
}

static int set_bone_component(BLB_Object3D *object, const char *name, int component, HMM_Vec3 value) {
  BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (!instance || !name)
    return -1;
  size_t index = find_node(instance->asset, name);
  if (index == SIZE_MAX || !instance->asset->nodes[index].is_bone)
    return -1;
  if (component == 0)
    instance->manual_positions[index] = value;
  else if (component == 1) {
    HMM_Mat4 rx = HMM_Rotate_RH(HMM_AngleDeg(value.x), HMM_V3(1.0f, 0.0f, 0.0f));
    HMM_Mat4 ry = HMM_Rotate_RH(HMM_AngleDeg(value.y), HMM_V3(0.0f, 1.0f, 0.0f));
    HMM_Mat4 rz = HMM_Rotate_RH(HMM_AngleDeg(value.z), HMM_V3(0.0f, 0.0f, 1.0f));
    instance->manual_rotations[index] = HMM_M4ToQ_RH(HMM_MulM4(rz, HMM_MulM4(ry, rx)));
  } else
    instance->manual_scales[index] = value;

  instance->manual_overrides[index] |= (uint8_t)(1u << component);
  BLB_Model_UpdateAnimation(object, 0.0f);
  return 0;
}

int BLB_Model_SetBonePosition(BLB_Object3D *object, const char *name, HMM_Vec3 position) { return set_bone_component(object, name, 0, position); }
int BLB_Model_SetBoneRotation(BLB_Object3D *object, const char *name, HMM_Vec3 rotation_degrees) {
  return set_bone_component(object, name, 1, rotation_degrees);
}
int BLB_Model_SetBoneScale(BLB_Object3D *object, const char *name, HMM_Vec3 scale) { return set_bone_component(object, name, 2, scale); }

int BLB_Model_SetBoneTransform(BLB_Object3D *object, const char *name, HMM_Vec3 position, HMM_Vec3 rotation_degrees, HMM_Vec3 scale) {
  BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (!instance || !name)
    return -1;
  size_t index = find_node(instance->asset, name);
  if (index == SIZE_MAX)
    return -1;
  HMM_Mat4 rx = HMM_Rotate_RH(HMM_AngleDeg(rotation_degrees.x), HMM_V3(1.0f, 0.0f, 0.0f));
  HMM_Mat4 ry = HMM_Rotate_RH(HMM_AngleDeg(rotation_degrees.y), HMM_V3(0.0f, 1.0f, 0.0f));
  HMM_Mat4 rz = HMM_Rotate_RH(HMM_AngleDeg(rotation_degrees.z), HMM_V3(0.0f, 0.0f, 1.0f));
  instance->manual_positions[index] = position;
  instance->manual_rotations[index] = HMM_M4ToQ_RH(HMM_MulM4(rz, HMM_MulM4(ry, rx)));
  instance->manual_scales[index] = scale;
  instance->manual_overrides[index] = BLB_ANIMATION_OVERRIDE_POSITION | BLB_ANIMATION_OVERRIDE_ROTATION | BLB_ANIMATION_OVERRIDE_SCALE;
  BLB_Model_UpdateAnimation(object, 0.0f);
  return 0;
}

int BLB_Model_ResetBone(BLB_Object3D *object, const char *name) {
  BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (!instance || !name)
    return -1;
  size_t index = find_node(instance->asset, name);
  if (index == SIZE_MAX || !instance->asset->nodes[index].is_bone)
    return -1;
  instance->manual_overrides[index] = false;
  BLB_Model_UpdateAnimation(object, 0.0f);
  return 0;
}

size_t BLB_Model_GetAnimationCount(const BLB_Object3D *object) {
  const BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  return instance ? instance->asset->clip_count : 0;
}

const char *BLB_Model_GetAnimationName(const BLB_Object3D *object, size_t index) {
  const BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (!instance || index >= instance->asset->clip_count)
    return NULL;
  return instance->asset->clips[index].name;
}

int BLB_Model_PlayAnimation(BLB_Object3D *object, const char *name, bool loop) {
  BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (!instance || !name)
    return -1;
  for (size_t i = 0; i < instance->asset->clip_count; ++i) {
    if (instance->asset->clips[i].name && strcmp(instance->asset->clips[i].name, name) == 0) {
      instance->active_clip = (int)i;
      instance->time_ticks = 0.0;
      instance->loop = loop;
      instance->playing = true;
      BLB_Model_UpdateAnimation(object, 0.0f);
      return 0;
    }
  }
  return -1;
}

void BLB_Model_StopAnimation(BLB_Object3D *object) {
  BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (!instance)
    return;
  instance->playing = false;
  instance->active_clip = -1;
  instance->time_ticks = 0.0;
  BLB_Model_UpdateAnimation(object, 0.0f);
}

void BLB_Model_SetAnimationSpeed(BLB_Object3D *object, float speed) {
  BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (instance && isfinite(speed))
    instance->speed = speed;
}

int BLB_Model_SetAnimationTime(BLB_Object3D *object, float seconds) {
  BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (!instance || instance->active_clip < 0 || (size_t)instance->active_clip >= instance->asset->clip_count || !isfinite(seconds) || seconds < 0.0f)
    return -1;
  instance->time_ticks = seconds * instance->asset->clips[instance->active_clip].ticks_per_second;
  BLB_Model_UpdateAnimation(object, 0.0f);
  return 0;
}

float BLB_Model_GetAnimationTime(const BLB_Object3D *object) {
  const BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (!instance || instance->active_clip < 0 || (size_t)instance->active_clip >= instance->asset->clip_count)
    return 0.0f;

  double rate = instance->asset->clips[instance->active_clip].ticks_per_second;
  return rate > 0.0 ? (float)(instance->time_ticks / rate) : 0.0f;
}

void BLB_Model_UpdateAnimation(BLB_Object3D *object, float delta_time) {
  BLB_ModelAnimationInstance *instance = object ? object->animation3d : NULL;
  if (!instance || !instance->asset || delta_time < 0.0f || !isfinite(delta_time))
    return;

  BLB_ModelAnimationAsset *asset = instance->asset;
  if (instance->playing && instance->active_clip >= 0 && (size_t)instance->active_clip < asset->clip_count) {
    BLB_ModelAnimationClip *clip = &asset->clips[instance->active_clip];
    instance->time_ticks += (double)delta_time * instance->speed * clip->ticks_per_second;
    if (clip->duration_ticks > 0.0) {
      if (instance->loop) {
        instance->time_ticks = fmod(instance->time_ticks, clip->duration_ticks);
        if (instance->time_ticks < 0.0)
          instance->time_ticks += clip->duration_ticks;
      } else if (instance->time_ticks >= clip->duration_ticks) {
        instance->time_ticks = clip->duration_ticks;
        instance->playing = false;
      } else if (instance->time_ticks < 0.0) {
        instance->time_ticks = 0.0;
      }
    }
  }

  for (size_t i = 0; i < asset->node_count; ++i) {
    instance->positions[i] = asset->nodes[i].bind_position;
    instance->rotations[i] = asset->nodes[i].bind_rotation;
    instance->scales[i] = asset->nodes[i].bind_scale;
  }

  if (instance->active_clip >= 0 && (size_t)instance->active_clip < asset->clip_count) {
    const BLB_ModelAnimationClip *clip = &asset->clips[instance->active_clip];
    for (size_t i = 0; i < clip->channel_count; ++i) {
      const BLB_ModelAnimationChannel *channel = &clip->channels[i];
      if (channel->node_index < 0 || (size_t)channel->node_index >= asset->node_count)
        continue;
      size_t node_index = (size_t)channel->node_index;
      uint8_t overrides = instance->manual_overrides[node_index];
      if (!(overrides & BLB_ANIMATION_OVERRIDE_POSITION))
        instance->positions[node_index] =
            sample_vector_keys(channel->positions, channel->position_count, instance->time_ticks, instance->positions[node_index]);
      if (!(overrides & BLB_ANIMATION_OVERRIDE_ROTATION))
        instance->rotations[node_index] =
            sample_quaternion_keys(channel->rotations, channel->rotation_count, instance->time_ticks, instance->rotations[node_index]);
      if (!(overrides & BLB_ANIMATION_OVERRIDE_SCALE))
        instance->scales[node_index] = sample_vector_keys(channel->scales, channel->scale_count, instance->time_ticks, instance->scales[node_index]);
    }
  }
  if (instance->transform_dirty)
    memset(instance->transform_dirty, 0, asset->node_count * sizeof(*instance->transform_dirty));

  if (instance->active_clip >= 0 && (size_t)instance->active_clip < asset->clip_count) {
    const BLB_ModelAnimationClip *clip = &asset->clips[instance->active_clip];
    for (size_t i = 0; i < clip->channel_count; ++i) {
      const BLB_ModelAnimationChannel *channel = &clip->channels[i];
      if (channel->node_index < 0 || (size_t)channel->node_index >= asset->node_count)
        continue;
      if (channel->position_count || channel->rotation_count || channel->scale_count)
        instance->transform_dirty[channel->node_index] = 1u;
    }
  }

  for (size_t i = 0; i < asset->node_count; ++i) {
    uint8_t overrides = instance->manual_overrides[i];
    if (overrides & BLB_ANIMATION_OVERRIDE_POSITION)
      instance->positions[i] = instance->manual_positions[i];
    if (overrides & BLB_ANIMATION_OVERRIDE_ROTATION)
      instance->rotations[i] = instance->manual_rotations[i];
    if (overrides & BLB_ANIMATION_OVERRIDE_SCALE)
      instance->scales[i] = instance->manual_scales[i];
    if (overrides)
      instance->transform_dirty[i] = 1u;
  }
  skin_instance(object, instance);
  object->optimization_dynamic = true;
  object->optimization_stable_frames = 0;
  object->optimization_static_recovered = false;
}
