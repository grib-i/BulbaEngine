#include "bulba/core/objects3d/model_loader_assimp.h"
#include "bulba/core/physics/physics3d/physics3d.h"
#include "bulba/core/utils/delta_time.h"

#include "bulba/core/objects3d/model_loader_common.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/object_lod.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/utils/config.h"
#include "bulba/core/utils/object.h"
#include "model_animation_internal.h"

/* Assimp C headers need ai_real before quaternion.h (included by anim.h). */
#include <assimp/types.h>

#include <assimp/anim.h>
#include <assimp/cimport.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <png.h>

#ifdef BLB_HAVE_JPEG
#include <jpeglib.h>
#include <setjmp.h>
#endif

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct BLB_ImportedModelAsset {
  struct BLB_ImportedModelAsset *next;
  char *path;
  BLB_ObjectSubmesh *submeshes;
  size_t submesh_count;
  BLB_Polygon3D *polygon;
  uint64_t geometry_id;
  size_t refs;
  BLB_ModelAnimationAsset *animation_asset;
} BLB_ImportedModelAsset;

static BLB_ImportedModelAsset *imported_model_assets;

static bool scene_has_skin(const struct aiScene *scene) {
  if (!scene)
    return false;

  for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    if (scene->mMeshes[i] && scene->mMeshes[i]->mNumBones > 0)
      return true;

  return false;
}

static size_t count_ai_nodes(const struct aiNode *node) {
  if (!node)
    return 0;

  size_t count = 1;

  for (unsigned int i = 0; i < node->mNumChildren; ++i)
    count += count_ai_nodes(node->mChildren[i]);

  return count;
}

static BLB_Mat4 ai_matrix_to_hmm(const struct aiMatrix4x4 *source) {
  BLB_Mat4 result = BLB_M4D(1.0f);

  if (!source)
    return result;

  result.Elements[0][0] = source->a1;
  result.Elements[1][0] = source->a2;
  result.Elements[2][0] = source->a3;
  result.Elements[3][0] = source->a4;
  result.Elements[0][1] = source->b1;
  result.Elements[1][1] = source->b2;
  result.Elements[2][1] = source->b3;
  result.Elements[3][1] = source->b4;
  result.Elements[0][2] = source->c1;
  result.Elements[1][2] = source->c2;
  result.Elements[2][2] = source->c3;
  result.Elements[3][2] = source->c4;
  result.Elements[0][3] = source->d1;
  result.Elements[1][3] = source->d2;
  result.Elements[2][3] = source->d3;
  result.Elements[3][3] = source->d4;

  return result;
}

static void decompose_hmm_matrix(BLB_Mat4 matrix, BLB_Vec3 *position, BLB_Quat *rotation, BLB_Vec3 *scale) {
  BLB_Vec3 axis_x = BLB_V3(matrix.Elements[0][0], matrix.Elements[0][1], matrix.Elements[0][2]);
  BLB_Vec3 axis_y = BLB_V3(matrix.Elements[1][0], matrix.Elements[1][1], matrix.Elements[1][2]);
  BLB_Vec3 axis_z = BLB_V3(matrix.Elements[2][0], matrix.Elements[2][1], matrix.Elements[2][2]);
  BLB_Vec3 actual_scale = BLB_V3(BLB_LenV3(axis_x), BLB_LenV3(axis_y), BLB_LenV3(axis_z));

  if (position)
    *position = BLB_V3(matrix.Elements[3][0], matrix.Elements[3][1], matrix.Elements[3][2]);

  if (scale)
    *scale = actual_scale;

  if (rotation) {
    BLB_Mat4 rotation_matrix = matrix;

    for (size_t row = 0; row < 3; ++row) {
      rotation_matrix.Elements[0][row] =
          actual_scale.x > 0.000001f ? rotation_matrix.Elements[0][row] / actual_scale.x : 0.0f;
      rotation_matrix.Elements[1][row] =
          actual_scale.y > 0.000001f ? rotation_matrix.Elements[1][row] / actual_scale.y : 0.0f;
      rotation_matrix.Elements[2][row] =
          actual_scale.z > 0.000001f ? rotation_matrix.Elements[2][row] / actual_scale.z : 0.0f;
    }

    rotation_matrix.Elements[3][0] = 0.0f;
    rotation_matrix.Elements[3][1] = 0.0f;
    rotation_matrix.Elements[3][2] = 0.0f;
    rotation_matrix.Elements[3][3] = 1.0f;
    *rotation = BLB_NormQ(BLB_M4ToQ_RH(rotation_matrix));
  }
}

static const char *animation_name_leaf(const char *name) {
  if (!name)
    return "";

  const char *leaf = name;

  for (const char *p = name; *p; ++p)
    if (*p == '|' || *p == ':' || *p == '/' || *p == '\\')
      leaf = p + 1;

  return leaf;
}

static int find_animation_node(const BLB_ModelAnimationAsset *asset, const char *name) {
  if (!asset || !name || !*name)
    return -1;

  for (size_t i = 0; i < asset->node_count; ++i)
    if (asset->nodes[i].name && strcmp(asset->nodes[i].name, name) == 0)
      return (int)i;

  const char *leaf = animation_name_leaf(name);

  if (!*leaf)
    return -1;

  for (size_t i = 0; i < asset->node_count; ++i) {
    const char *node_name = asset->nodes[i].name;

    if (node_name && strcmp(animation_name_leaf(node_name), leaf) == 0)
      return (int)i;
  }

  return -1;
}

#ifndef ASSIMP_BUILD_NO_ARMATUREPOPULATE_PROCESS
static int find_animation_node_by_source(const BLB_ModelAnimationAsset *asset, const struct aiNode *source_node) {
  if (!asset || !source_node)
    return -1;

  for (size_t i = 0; i < asset->node_count; ++i)
    if (asset->nodes[i].source_node == source_node)
      return (int)i;

  return -1;
}
#endif

static BLB_Mat4 compose_node_trs(BLB_Vec3 position, BLB_Quat rotation, BLB_Vec3 scale) {
  BLB_Mat4 translation = BLB_Translate(position);
  BLB_Mat4 rotate = BLB_QToM4(rotation);
  BLB_Mat4 scaling = BLB_Scale(scale);

  return BLB_MulM4(translation, BLB_MulM4(rotate, scaling));
}

static bool matrix_is_finite(BLB_Mat4 matrix) {
  for (size_t column = 0; column < 4; ++column)
    for (size_t row = 0; row < 4; ++row)
      if (!isfinite(matrix.Elements[column][row]))
        return false;

  return true;
}

static float matrix_max_abs(BLB_Mat4 matrix) {
  float maximum = 0.0f;

  for (size_t column = 0; column < 4; ++column)
    for (size_t row = 0; row < 4; ++row)
      maximum = fmaxf(maximum, fabsf(matrix.Elements[column][row]));

  return maximum;
}

static float matrix_relative_error(BLB_Mat4 a, BLB_Mat4 b) {
  float difference = 0.0f;

  for (size_t column = 0; column < 4; ++column)
    for (size_t row = 0; row < 4; ++row)
      difference = fmaxf(difference, fabsf(a.Elements[column][row] - b.Elements[column][row]));

  return difference / fmaxf(1.0f, fmaxf(matrix_max_abs(a), matrix_max_abs(b)));
}

static bool build_ai_node_tree(BLB_ModelAnimationAsset *asset, const struct aiNode *source, int parent_index,
                               BLB_Mat4 parent_global, int *mesh_node_indices, size_t mesh_count, size_t *cursor,
                               bool preserve_bind_residual) {
  if (!asset || !source || !cursor || *cursor >= asset->node_count)
    return false;

  size_t index = (*cursor)++;
  BLB_ModelAnimationNode *node = &asset->nodes[index];
  node->name = BLB_ModelDuplicateString(source->mName.data);

  if (!node->name)
    return false;

  node->source_node = source;
  node->parent_index = parent_index;

  BLB_Mat4 local = ai_matrix_to_hmm(&source->mTransformation);
  node->bind_local = local;
  node->bind_global = parent_index >= 0 ? BLB_MulM4(parent_global, local) : local;

  decompose_hmm_matrix(local, &node->bind_position, &node->bind_rotation, &node->bind_scale);
  node->bind_residual = BLB_M4D(1.0f);

  if (preserve_bind_residual) {
    BLB_Mat4 bind_trs = compose_node_trs(node->bind_position, node->bind_rotation, node->bind_scale);
    BLB_Mat4 bind_trs_inverse = BLB_InvGeneralM4(bind_trs);

    if (matrix_is_finite(bind_trs_inverse)) {
      BLB_Mat4 residual = BLB_MulM4(bind_trs_inverse, local);

      if (matrix_is_finite(residual))
        node->bind_residual = residual;
    }
  }

  for (unsigned int i = 0; i < source->mNumMeshes; ++i) {
    unsigned int mesh_index = source->mMeshes[i];

    if (mesh_index < mesh_count && mesh_node_indices[mesh_index] < 0)
      mesh_node_indices[mesh_index] = (int)index;
  }

  for (unsigned int i = 0; i < source->mNumChildren; ++i)
    if (!build_ai_node_tree(asset, source->mChildren[i], (int)index, node->bind_global, mesh_node_indices, mesh_count,
                            cursor, preserve_bind_residual))
      return false;

  return true;
}

static bool import_animation_clips(BLB_ModelAnimationAsset *asset, const struct aiScene *scene) {
  if (!asset || !scene || scene->mNumAnimations == 0)
    return true;

  asset->clips = calloc(scene->mNumAnimations, sizeof(*asset->clips));

  if (!asset->clips)
    return false;

  asset->clip_count = scene->mNumAnimations;

  for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
    const struct aiAnimation *source = scene->mAnimations[i];
    BLB_ModelAnimationClip *clip = &asset->clips[i];

    if (!source)
      continue;

    char generated_name[48];
    snprintf(generated_name, sizeof(generated_name), "Animation_%u", i);
    clip->name = BLB_ModelDuplicateString(source->mName.length ? source->mName.data : generated_name);
    clip->duration_ticks = source->mDuration;
    clip->ticks_per_second = source->mTicksPerSecond > 0.0 ? source->mTicksPerSecond : 25.0;
    clip->channel_count = source->mNumChannels;
    clip->channels = calloc(clip->channel_count ? clip->channel_count : 1u, sizeof(*clip->channels));

    if (!clip->name || !clip->channels)
      return false;

    for (unsigned int j = 0; j < source->mNumChannels; ++j) {
      const struct aiNodeAnim *source_channel = source->mChannels[j];
      BLB_ModelAnimationChannel *channel = &clip->channels[j];
      channel->node_index = source_channel ? find_animation_node(asset, source_channel->mNodeName.data) : -1;

      if (!source_channel || channel->node_index < 0)
        continue;

      channel->position_count = source_channel->mNumPositionKeys;
      channel->rotation_count = source_channel->mNumRotationKeys;
      channel->scale_count = source_channel->mNumScalingKeys;

      if (channel->position_count) {
        channel->positions = calloc(channel->position_count, sizeof(*channel->positions));

        if (!channel->positions)
          return false;

        for (unsigned int k = 0; k < source_channel->mNumPositionKeys; ++k) {
          channel->positions[k].time = source_channel->mPositionKeys[k].mTime;

          const struct aiVector3D *value = &source_channel->mPositionKeys[k].mValue;
          channel->positions[k].value = BLB_V3(value->x, value->y, value->z);

          if (channel->positions[k].time > clip->duration_ticks)
            clip->duration_ticks = channel->positions[k].time;
        }
      }

      if (channel->rotation_count) {
        channel->rotations = calloc(channel->rotation_count, sizeof(*channel->rotations));

        if (!channel->rotations)
          return false;

        for (unsigned int k = 0; k < source_channel->mNumRotationKeys; ++k) {
          channel->rotations[k].time = source_channel->mRotationKeys[k].mTime;

          const struct aiQuaternion *value = &source_channel->mRotationKeys[k].mValue;
          channel->rotations[k].value = BLB_NormQ(BLB_Q(value->x, value->y, value->z, value->w));

          if (channel->rotations[k].time > clip->duration_ticks)
            clip->duration_ticks = channel->rotations[k].time;
        }
      }

      if (channel->scale_count) {
        channel->scales = calloc(channel->scale_count, sizeof(*channel->scales));

        if (!channel->scales)
          return false;

        for (unsigned int k = 0; k < source_channel->mNumScalingKeys; ++k) {
          channel->scales[k].time = source_channel->mScalingKeys[k].mTime;

          const struct aiVector3D *value = &source_channel->mScalingKeys[k].mValue;
          channel->scales[k].value = BLB_V3(value->x, value->y, value->z);

          if (channel->scales[k].time > clip->duration_ticks)
            clip->duration_ticks = channel->scales[k].time;
        }
      }
    }
  }

  return true;
}

static BLB_ModelAnimationAsset *build_animation_asset(const struct aiScene *scene, int **mesh_node_indices_out,
                                                      bool *needs_animation_out, bool preserve_bind_residual) {

  if (!scene || !scene->mRootNode || !mesh_node_indices_out || !needs_animation_out)
    return NULL;

  *mesh_node_indices_out = NULL;
  *needs_animation_out = scene_has_skin(scene);
  BLB_ModelAnimationAsset *asset = calloc(1, sizeof(*asset));

  if (!asset)
    return NULL;

  asset->node_count = count_ai_nodes(scene->mRootNode);
  asset->nodes = calloc(asset->node_count, sizeof(*asset->nodes));
  int *mesh_node_indices = malloc(scene->mNumMeshes * sizeof(*mesh_node_indices));

  if (!asset->nodes || !mesh_node_indices)
    goto fail;

  for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    mesh_node_indices[i] = -1;

  size_t cursor = 0;

  if (!build_ai_node_tree(asset, scene->mRootNode, -1, BLB_M4D(1.0f), mesh_node_indices, scene->mNumMeshes, &cursor,
                          preserve_bind_residual) ||
      cursor != asset->node_count)
    goto fail;

  asset->root_inverse = BLB_InvGeneralM4(asset->nodes[0].bind_global);

  for (size_t i = 0; i < 4; ++i)
    for (size_t j = 0; j < 4; ++j)
      if (!isfinite(asset->root_inverse.Elements[i][j]))
        asset->root_inverse = BLB_M4D(1.0f);

  if (!import_animation_clips(asset, scene))
    goto fail;

  if (*needs_animation_out) {
    asset->skins = calloc(scene->mNumMeshes, sizeof(*asset->skins));

    if (!asset->skins)
      goto fail;
    asset->skin_capacity = scene->mNumMeshes;
  }

  *mesh_node_indices_out = mesh_node_indices;

  return asset;
fail:
  free(mesh_node_indices);
  BLB_ModelAnimationAsset_Destroy(asset);

  return NULL;
}

static void add_vertex_influence(BLB_ModelSkinVertex *vertex, uint32_t node_index, float weight) {
  if (!vertex || !isfinite(weight) || weight <= 0.0f)
    return;

  for (size_t i = 0; i < 4; ++i) {
    if (vertex->weights[i] > 0.0f && vertex->node_indices[i] == node_index) {
      vertex->weights[i] += weight;

      return;
    }

    if (vertex->weights[i] <= 0.0f) {
      vertex->node_indices[i] = node_index;
      vertex->weights[i] = weight;

      return;
    }
  }
  size_t smallest = 0;

  for (size_t i = 1; i < 4; ++i)
    if (vertex->weights[i] < vertex->weights[smallest])
      smallest = i;

  if (weight > vertex->weights[smallest]) {
    vertex->node_indices[smallest] = node_index;
    vertex->weights[smallest] = weight;
  }
}

static bool import_skin_submesh(BLB_ModelAnimationAsset *asset, const struct aiMesh *mesh, int mesh_node_index,
                                bool repair_fbx_bind_offsets) {
  if (!asset || !mesh || !asset->skins || asset->skin_count >= asset->skin_capacity)
    return false;

  BLB_ModelSkinSubmesh *skin = &asset->skins[asset->skin_count];
  skin->vertex_count = mesh->mNumVertices;
  skin->vertices = calloc(skin->vertex_count, sizeof(*skin->vertices));
  skin->offsets = calloc(asset->node_count, sizeof(*skin->offsets));

  if (!skin->vertices || !skin->offsets) {
    free(skin->vertices);
    free(skin->offsets);
    memset(skin, 0, sizeof(*skin));

    return false;
  }

  for (size_t i = 0; i < asset->node_count; ++i)
    skin->offsets[i] = BLB_M4D(1.0f);

  if (mesh->mNumBones > 0) {
    for (unsigned int i = 0; i < mesh->mNumBones; ++i) {
      const struct aiBone *bone = mesh->mBones[i];

      if (!bone)
        continue;

      int node_index = -1;
#ifndef ASSIMP_BUILD_NO_ARMATUREPOPULATE_PROCESS
      node_index = find_animation_node_by_source(asset, bone->mNode);
#endif
      if (node_index < 0)
        node_index = find_animation_node(asset, bone->mName.data);

      if (node_index < 0) {
        if (BLB_DEBUG)
          fprintf(stderr,
                  "[BLB:ML_Assimp] Model bone '%s' has no matching scene node; its "
                  "weights are ignored.\n",
                  bone->mName.data);
        continue;
      }

      asset->nodes[node_index].is_bone = true;

      BLB_Mat4 offset = ai_matrix_to_hmm(&bone->mOffsetMatrix);

      if (repair_fbx_bind_offsets && mesh_node_index >= 0 && (size_t)mesh_node_index < asset->node_count) {
        BLB_Mat4 expected_mesh_global = asset->nodes[mesh_node_index].bind_global;
        BLB_Mat4 bone_bind_global = asset->nodes[node_index].bind_global;
        BLB_Mat4 actual_mesh_global = BLB_MulM4(bone_bind_global, offset);

        if (!matrix_is_finite(actual_mesh_global) ||
            matrix_relative_error(actual_mesh_global, expected_mesh_global) > 0.02f) {
          BLB_Mat4 inverse_bone_bind = BLB_InvGeneralM4(bone_bind_global);

          if (matrix_is_finite(inverse_bone_bind)) {
            BLB_Mat4 repaired = BLB_MulM4(inverse_bone_bind, expected_mesh_global);

            if (matrix_is_finite(repaired)) {
              offset = repaired;
            }
          }
        }
      }

      skin->offsets[node_index] = offset;

      for (unsigned int j = 0; j < bone->mNumWeights; ++j) {
        const struct aiVertexWeight *weight = &bone->mWeights[j];

        if (weight->mVertexId < skin->vertex_count)
          add_vertex_influence(&skin->vertices[weight->mVertexId], (uint32_t)node_index, weight->mWeight);
      }
    }
  }
  uint32_t rigid_node =
      mesh_node_index >= 0 && (size_t)mesh_node_index < asset->node_count ? (uint32_t)mesh_node_index : 0u;

  for (size_t i = 0; i < skin->vertex_count; ++i) {
    BLB_ModelSkinVertex *vertex = &skin->vertices[i];
    float total = 0.0f;

    for (size_t j = 0; j < 4; ++j)
      total += vertex->weights[j];

    if (total <= 0.000001f) {
      vertex->node_indices[0] = rigid_node;
      vertex->weights[0] = 1.0f;
      total = 1.0f;
    }

    for (size_t j = 0; j < 4; ++j)
      vertex->weights[j] /= total;
  }

  ++asset->skin_count;

  return true;
}

static void imported_asset_destroy(BLB_ImportedModelAsset *asset) {
  if (!asset)
    return;

  for (size_t i = 0; i < asset->submesh_count; ++i) {
    BLB_ModelFreePolygon(asset->submeshes[i].polygon);

    if (asset->submeshes[i].material)
      BLB_Material_Release(asset->submeshes[i].material);
  }

  free(asset->submeshes);
  BLB_ModelFreePolygon(asset->polygon);
  BLB_ModelAnimationAsset_Destroy(asset->animation_asset);
  free(asset->path);
  free(asset);
}

static void imported_asset_remove(BLB_ImportedModelAsset *asset) {
  BLB_ImportedModelAsset **cursor = &imported_model_assets;

  while (*cursor) {
    if (*cursor == asset) {
      *cursor = asset->next;
      asset->next = NULL;

      return;
    }
    cursor = &(*cursor)->next;
  }
}

static void imported_asset_destroy_if_unused(BLB_ImportedModelAsset *asset) {
  if (!asset || asset->refs != 0)
    return;

  imported_asset_remove(asset);
  imported_asset_destroy(asset);
}

static void imported_asset_release(BLB_ImportedModelAsset *asset) {
  if (!asset || asset->refs == 0)
    return;

  --asset->refs;

  if (asset->refs == 0) {
    imported_asset_remove(asset);
    imported_asset_destroy(asset);
  }
}

static BLB_ImportedModelAsset *imported_asset_find(const char *path) {
  for (BLB_ImportedModelAsset *asset = imported_model_assets; asset; asset = asset->next) {
    if (strcmp(asset->path, path) == 0)
      return asset;
  }

  return NULL;
}

static BLB_ImportedModelAsset *imported_asset_take_object(BLB_Object3D *object, const char *path,
                                                          BLB_ModelAnimationAsset *animation_asset) {
  if (!object || !path || !object->submeshes || object->submesh_count == 0 || !object->polygon)
    return NULL;

  BLB_ImportedModelAsset *asset = calloc(1, sizeof(*asset));

  if (!asset)
    return NULL;

  asset->path = BLB_ModelDuplicateString(path);

  if (!asset->path) {
    free(asset);

    return NULL;
  }

  asset->submeshes = object->submeshes;
  asset->submesh_count = object->submesh_count;
  asset->polygon = object->polygon;
  asset->geometry_id = object->geometry_id;
  asset->animation_asset = animation_asset;

  if (object->material)
    BLB_Material_Release(object->material);

  object->material = NULL;
  object->submeshes = NULL;
  object->submesh_count = 0;
  object->polygon = NULL;
  object->geometry_id = 0;
  memset(&object->mesh, 0, sizeof(object->mesh));

  asset->next = imported_model_assets;
  imported_model_assets = asset;

  return asset;
}

static void imported_texture_retain(BLB_Texture *texture, void *user_data) {
  (void)user_data;
  BLB_Texture_Retain(texture);
}

static void imported_texture_release(BLB_Texture *texture, void *user_data) {
  (void)user_data;
  BLB_Texture_Release(texture);
}

static BLB_Object3D *instantiate_imported_asset(BLB_ImportedModelAsset *asset, BLB_Vec3 scale, BLB_Vec3 position,
                                                BLB_Texture *texture, BLB_TextureProjection mapping) {
  if (!asset || asset->submesh_count == 0 || !asset->polygon)
    return NULL;

  BLB_Object3D *object = calloc(1, sizeof(*object));

  if (!object) {
    imported_asset_destroy_if_unused(asset);

    return NULL;
  }

  object->type = BLB_OBJECT_CUSTOM_MODEL;

  if (!BLB_OBJECTS_ID || object->type >= BLB_OBJECTS_ID_COUNT) {
    free(object);
    imported_asset_destroy_if_unused(asset);

    return NULL;
  }

  BLB_ObjectID *id = &BLB_OBJECTS_ID[object->type];

  if (id->id == BLB_INVALID_OBJECT_ID) {
    free(object);
    imported_asset_destroy_if_unused(asset);

    return NULL;
  }

  object->id = id;
  ++id->id;
  object->model_asset = asset;
  ++asset->refs;
  object->delta_time = BLB_DeltaTime_Get();
  object->type = BLB_OBJECT_CUSTOM_MODEL;
  object->polygon = asset->polygon;
  object->geometry_id = asset->geometry_id;
  object->mesh.vertices = asset->polygon->vertices;
  object->mesh.normals = asset->polygon->normals;
  object->mesh.uvs = asset->polygon->uvs;
  object->mesh.vertex_count = asset->polygon->vertex_count;
  object->mesh.indices = asset->polygon->indices;
  object->mesh.index_count = asset->polygon->index_count;
  object->position = position;
  object->rotation = BLB_V3(0.0f, 0.0f, 0.0f);
  object->scale = scale;
  object->render_mode = BLB_RENDER_OPAQUE;
  object->color = BLB_COLOR_WHITE;
  object->visible = true;
  object->entity_id = BLB_INVALID_ENTITY_ID;
  object->component_mask = BLB_COMPONENT_TRANSFORM | BLB_COMPONENT_RENDERABLE;

  for (int i = 0; i < BLB_TRANSFORM_TYPE_COUNT; ++i)
    object->physics_sync_mode[i] = BLB_PHYSICS_SYNC_BIDIRECTIONAL;

  object->submeshes = calloc(asset->submesh_count, sizeof(*object->submeshes));

  if (!object->submeshes)
    goto fail;

  for (size_t i = 0; i < asset->submesh_count; ++i) {
    object->submeshes[i].polygon = asset->submeshes[i].polygon;
    object->submeshes[i].material = BLB_Material_Clone(asset->submeshes[i].material);

    if (!object->submeshes[i].material)
      goto fail;
    ++object->submesh_count;
  }

  object->material = object->submeshes[0].material;
  BLB_Material_Retain(object->material);

  if (texture) {
    if (BLB_Object3D_SetTexture(object, texture) != 0)
      goto fail;

    for (size_t i = 0; i < object->submesh_count; ++i) {
      BLB_Material *material = object->submeshes[i].material;
      BLB_MaterialTexture_SetProjection(&material->base_color_texture, mapping);
      BLB_Material_SetBaseColor(material, BLB_ColorRGBA(255, 255, 255, material->base_color.a));
    }
  }

  if (asset->animation_asset && BLB_ModelAnimationInstance_Create(object, asset->animation_asset) != 0)
    goto fail;

  BLB_Object3D_Transform(object, position, object->rotation, scale);

  return object;
fail:
  BLB_DestroyModelImported(object);

  return NULL;
}

static bool add_size(size_t a, size_t b, size_t *out) {
  if (a > SIZE_MAX - b)
    return false;

  *out = a + b;

  return true;
}

static char *model_path_directory(const char *path) {
  if (!path)
    return NULL;

  const char *slash = strrchr(path, '/');
  const char *backslash = strrchr(path, '\\');

  if (!slash || (backslash && backslash > slash))
    slash = backslash;

  if (!slash)
    return BLB_ModelDuplicateString(".");

  size_t length = (size_t)(slash - path);

  if (length == 0)
    length = 1;

  char *directory = malloc(length + 1u);

  if (!directory)
    return NULL;

  memcpy(directory, path, length);
  directory[length] = '\0';

  return directory;
}

static bool path_is_absolute(const char *path) {
  if (!path || !path[0])
    return false;

  if (path[0] == '/' || path[0] == '\\')
    return true;

  return isalpha((unsigned char)path[0]) && path[1] == ':';
}

static bool texture_path_exists(const char *path) {
  if (!path || !path[0])
    return false;

  FILE *probe = fopen(path, "rb");

  if (!probe)
    return false;

  fclose(probe);

  return true;
}

static char *join_texture_path(const char *directory, const char *name) {
  if (!directory || !name || !name[0])
    return NULL;

  size_t directory_length = strlen(directory);
  size_t name_length = strlen(name);

  if (directory_length > SIZE_MAX - name_length - 2u)
    return NULL;

  char *joined = malloc(directory_length + name_length + 2u);

  if (!joined)
    return NULL;

  snprintf(joined, directory_length + name_length + 2u, "%s/%s", directory, name);

  for (char *cursor = joined; *cursor; ++cursor)
    if (*cursor == '\\')
      *cursor = '/';

  return joined;
}

static char *resolve_texture_path(const char *model_path, const char *texture_path) {
  if (!texture_path || !texture_path[0])
    return NULL;

  char *resolved = NULL;
  char *directory = model_path_directory(model_path);

  if (!directory)
    return NULL;

  if (path_is_absolute(texture_path))
    resolved = BLB_ModelDuplicateString(texture_path);
  else
    resolved = join_texture_path(directory, texture_path);

  if (resolved) {
    for (char *cursor = resolved; *cursor; ++cursor)
      if (*cursor == '\\')
        *cursor = '/';
    if (texture_path_exists(resolved)) {
      free(directory);

      return resolved;
    }
  }

  const char *basename = texture_path;
  const char *slash = strrchr(texture_path, '/');
  const char *backslash = strrchr(texture_path, '\\');

  if (!slash || (backslash && backslash > slash))
    slash = backslash;

  if (slash && slash[1])
    basename = slash + 1;

  if (basename[0] && strcmp(basename, texture_path) != 0) {
    char *sibling = join_texture_path(directory, basename);

    if (sibling && texture_path_exists(sibling)) {
      free(resolved);
      free(directory);

      return sibling;
    }
    free(sibling);
  }

  if (texture_path_exists(texture_path)) {
    free(resolved);
    free(directory);

    return BLB_ModelDuplicateString(texture_path);
  }

  if (basename[0] && strcmp(basename, texture_path) != 0 && texture_path_exists(basename)) {
    free(resolved);
    free(directory);

    return BLB_ModelDuplicateString(basename);
  }

  free(directory);

  return resolved;
}

#ifdef BLB_HAVE_JPEG
typedef struct {
  struct jpeg_error_mgr base;
  jmp_buf jump;
} BLB_EmbeddedJPEGError;

typedef struct {
  struct jpeg_decompress_struct decoder;
  BLB_EmbeddedJPEGError error;
  unsigned char *rgba;
  unsigned char *row;
  bool created;
} BLB_EmbeddedJPEGContext;

static void embedded_jpeg_fail(j_common_ptr decoder) {
  BLB_EmbeddedJPEGError *error = (BLB_EmbeddedJPEGError *)decoder->err;
  longjmp(error->jump, 1);
}

static BLB_Texture *load_embedded_jpeg(const struct aiTexture *source) {
  BLB_EmbeddedJPEGContext *context = calloc(1, sizeof(*context));

  if (!context)
    return NULL;

  context->decoder.err = jpeg_std_error(&context->error.base);
  context->error.base.error_exit = embedded_jpeg_fail;

  if (setjmp(context->error.jump)) {
    if (context->created)
      jpeg_destroy_decompress(&context->decoder);

    free(context->rgba);
    free(context->row);
    free(context);

    return NULL;
  }

  jpeg_create_decompress(&context->decoder);
  context->created = true;
  jpeg_mem_src(&context->decoder, (unsigned char *)source->pcData, (unsigned long)source->mWidth);

  if (jpeg_read_header(&context->decoder, TRUE) != JPEG_HEADER_OK)
    longjmp(context->error.jump, 1);

  context->decoder.out_color_space = JCS_RGB;
  jpeg_start_decompress(&context->decoder);
  const size_t width = (size_t)context->decoder.output_width;
  const size_t height = (size_t)context->decoder.output_height;
  const size_t components = (size_t)context->decoder.output_components;

  if (!width || !height || components != 3u || width > SIZE_MAX / 4u || width * 4u > SIZE_MAX / height ||
      width > SIZE_MAX / components)
    longjmp(context->error.jump, 1);

  const size_t pixel_size = width * height * 4u;
  context->rgba = malloc(pixel_size);
  context->row = malloc(width * components);

  if (!context->rgba || !context->row)
    longjmp(context->error.jump, 1);

  while (context->decoder.output_scanline < context->decoder.output_height) {
    JSAMPROW row = context->row;
    const size_t y = (size_t)context->decoder.output_scanline;

    if (jpeg_read_scanlines(&context->decoder, &row, 1) != 1)
      longjmp(context->error.jump, 1);
    unsigned char *destination = context->rgba + y * width * 4u;

    for (size_t x = 0; x < width; ++x) {
      destination[x * 4u + 0u] = context->row[x * 3u + 0u];
      destination[x * 4u + 1u] = context->row[x * 3u + 1u];
      destination[x * 4u + 2u] = context->row[x * 3u + 2u];
      destination[x * 4u + 3u] = 255u;
    }
  }

  const uint32_t image_width = (uint32_t)width;
  const uint32_t image_height = (uint32_t)height;
  jpeg_finish_decompress(&context->decoder);
  jpeg_destroy_decompress(&context->decoder);
  context->created = false;
  BLB_Texture *texture = BLB_Texture_Create2D(image_width, image_height, context->rgba, pixel_size);
  free(context->rgba);
  free(context->row);
  free(context);

  return texture;
}
#endif

static BLB_Texture *load_embedded_texture(const struct aiTexture *source) {
  if (!source || !source->pcData || source->mWidth == 0)
    return NULL;

  if (source->mHeight > 0) {
    const size_t width = (size_t)source->mWidth;
    const size_t height = (size_t)source->mHeight;

    if (width > SIZE_MAX / height || width * height > SIZE_MAX / 4u)
      return NULL;

    const size_t pixel_size = width * height * 4u;
    unsigned char *pixels = malloc(pixel_size);

    if (!pixels)
      return NULL;

    for (size_t i = 0; i < width * height; ++i) {
      pixels[i * 4u + 0u] = source->pcData[i].r;
      pixels[i * 4u + 1u] = source->pcData[i].g;
      pixels[i * 4u + 2u] = source->pcData[i].b;
      pixels[i * 4u + 3u] = source->pcData[i].a;
    }

    BLB_Texture *texture = BLB_Texture_Create2D((uint32_t)width, (uint32_t)height, pixels, pixel_size);
    free(pixels);

    return texture;
  }

  png_image image;
  memset(&image, 0, sizeof(image));
  image.version = PNG_IMAGE_VERSION;

  if (!png_image_begin_read_from_memory(&image, source->pcData, (size_t)source->mWidth)) {
    png_image_free(&image);
#ifdef BLB_HAVE_JPEG
    return load_embedded_jpeg(source);
#else
    return NULL;
#endif
  }

  image.format = PNG_FORMAT_RGBA;
  const size_t pixel_size = PNG_IMAGE_SIZE(image);
  unsigned char *pixels = malloc(pixel_size);

  if (!pixels) {
    png_image_free(&image);

    return NULL;
  }

  if (!png_image_finish_read(&image, NULL, pixels, 0, NULL)) {
    free(pixels);
    png_image_free(&image);

    return NULL;
  }

  BLB_Texture *texture = BLB_Texture_Create2D(image.width, image.height, pixels, pixel_size);
  free(pixels);
  png_image_free(&image);

  return texture;
}

static BLB_Texture *load_material_texture(const struct aiScene *scene, const struct aiMaterial *material,
                                          enum aiTextureType type, unsigned int index, const char *model_path,
                                          enum aiTextureMapMode map_modes[3], unsigned int *uv_index) {
  struct aiString path;
  memset(&path, 0, sizeof(path));

  if (aiGetMaterialTexture(material, type, index, &path, NULL, uv_index, NULL, NULL, map_modes, NULL) !=
      aiReturn_SUCCESS)
    return NULL;

  const char *name = path.data;

  if (name[0] == '*') {
    char *end = NULL;
    unsigned long embedded_index = strtoul(name + 1, &end, 10);

    if (end == name + 1 || *end != '\0' || embedded_index >= scene->mNumTextures)
      return NULL;

    return load_embedded_texture(scene->mTextures[embedded_index]);
  }

  char *resolved = resolve_texture_path(model_path, name);

  if (!resolved)
    return NULL;

  BLB_Texture *texture = BLB_Texture_Load2D(resolved);

  if (!texture && BLB_DEBUG)
    fprintf(stderr, "[BLB:ML_Assimp] Model '%s' references a missing or unsupported texture '%s'\n",
            model_path ? model_path : "<unknown>", resolved);
  free(resolved);

  return texture;
}

static void set_material_texture(BLB_Material *material, BLB_MaterialTexture *slot, BLB_Texture *texture,
                                 unsigned int uv_index, const enum aiTextureMapMode map_modes[3]) {
  if (!material || !slot || !texture)
    return;

  slot->uv_set = (int)uv_index;
  slot->wrap_u = BLB_TEXTURE_WRAP_REPEAT;
  slot->wrap_v = BLB_TEXTURE_WRAP_REPEAT;

  if (map_modes) {
    if (map_modes[0] == aiTextureMapMode_Clamp || map_modes[0] == aiTextureMapMode_Decal)
      slot->wrap_u = BLB_TEXTURE_WRAP_CLAMP_TO_EDGE;
    else if (map_modes[0] == aiTextureMapMode_Mirror)
      slot->wrap_u = BLB_TEXTURE_WRAP_MIRRORED_REPEAT;

    if (map_modes[1] == aiTextureMapMode_Clamp || map_modes[1] == aiTextureMapMode_Decal)
      slot->wrap_v = BLB_TEXTURE_WRAP_CLAMP_TO_EDGE;
    else if (map_modes[1] == aiTextureMapMode_Mirror)
      slot->wrap_v = BLB_TEXTURE_WRAP_MIRRORED_REPEAT;
  }

  BLB_MaterialTexture_AdoptTexture(slot, texture);
  (void)material;
}

static BLB_Texture *load_first_material_texture(const struct aiScene *scene, const struct aiMaterial *source,
                                                const char *model_path, enum aiTextureType primary,
                                                enum aiTextureType fallback, enum aiTextureMapMode map_modes[3],
                                                unsigned int *uv_index) {
  BLB_Texture *texture = load_material_texture(scene, source, primary, 0, model_path, map_modes, uv_index);

  if (!texture && fallback != primary && fallback != aiTextureType_NONE) {
    map_modes[0] = aiTextureMapMode_Wrap;
    map_modes[1] = aiTextureMapMode_Wrap;
    *uv_index = 0;
    texture = load_material_texture(scene, source, fallback, 0, model_path, map_modes, uv_index);
  }

  return texture;
}

static BLB_Material *import_material(const struct aiScene *scene, const struct aiMaterial *source,
                                     const char *model_path, BLB_TextureProjection fallback_mapping,
                                     bool mesh_has_uvs) {
  BLB_Material *material = BLB_Material_Create3D();

  if (!material)
    return NULL;

  material->texture_retain = imported_texture_retain;
  material->texture_release = imported_texture_release;
  material->texture_user_data = NULL;
  struct aiString imported_name;

  if (aiGetMaterialString(source, AI_MATKEY_NAME, &imported_name) == aiReturn_SUCCESS)
    BLB_Material_SetName(material, imported_name.data);

  struct aiColor4D color = {1.0f, 1.0f, 1.0f, 1.0f};
  const char *extension = model_path ? strrchr(model_path, '.') : NULL;
  bool is_gltf_material = false;

  if (extension) {
    const size_t extension_length = strlen(extension);

    if (extension_length == 4u) {
      char e1 = (char)tolower((unsigned char)extension[1]);
      char e2 = (char)tolower((unsigned char)extension[2]);
      char e3 = (char)tolower((unsigned char)extension[3]);
      is_gltf_material = e1 == 'g' && e2 == 'l' && e3 == 'b';
    } else if (extension_length == 5u) {
      char e1 = (char)tolower((unsigned char)extension[1]);
      char e2 = (char)tolower((unsigned char)extension[2]);
      char e3 = (char)tolower((unsigned char)extension[3]);
      char e4 = (char)tolower((unsigned char)extension[4]);
      is_gltf_material = e1 == 'g' && e2 == 'l' && e3 == 't' && e4 == 'f';
    }
  }

  struct aiColor4D imported_color = {1.0f, 1.0f, 1.0f, 1.0f};
  aiReturn color_result = aiReturn_FAILURE;

  if (is_gltf_material) {
    color_result = aiGetMaterialColor(source, AI_MATKEY_BASE_COLOR, &imported_color);

    if (color_result != aiReturn_SUCCESS)
      color_result = aiGetMaterialColor(source, AI_MATKEY_COLOR_DIFFUSE, &imported_color);
  } else {
    color_result = aiGetMaterialColor(source, AI_MATKEY_COLOR_DIFFUSE, &imported_color);

    if (color_result != aiReturn_SUCCESS)
      color_result = aiGetMaterialColor(source, AI_MATKEY_BASE_COLOR, &imported_color);
  }

  if (color_result == aiReturn_SUCCESS)
    color = imported_color;

  if (!isfinite(color.r) || !isfinite(color.g) || !isfinite(color.b) || !isfinite(color.a))
    color = (struct aiColor4D){1.0f, 1.0f, 1.0f, 1.0f};

  color.r = fminf(1.0f, fmaxf(0.0f, color.r));
  color.g = fminf(1.0f, fmaxf(0.0f, color.g));
  color.b = fminf(1.0f, fmaxf(0.0f, color.b));
  color.a = fminf(1.0f, fmaxf(0.0f, color.a));

  float opacity = 1.0f;
  unsigned int value_count = 1;

  if (aiGetMaterialFloatArray(source, AI_MATKEY_OPACITY, &opacity, &value_count) != aiReturn_SUCCESS ||
      !isfinite(opacity))
    opacity = color.a;
  opacity = fminf(1.0f, fmaxf(0.0f, opacity));

  float metallic = 0.0f;
  value_count = 1;
  (void)aiGetMaterialFloatArray(source, AI_MATKEY_METALLIC_FACTOR, &metallic, &value_count);
  float roughness = 0.65f;
  value_count = 1;

  if (aiGetMaterialFloatArray(source, AI_MATKEY_ROUGHNESS_FACTOR, &roughness, &value_count) != aiReturn_SUCCESS) {
    float shininess = 0.0f;
    value_count = 1;

    if (aiGetMaterialFloatArray(source, AI_MATKEY_SHININESS, &shininess, &value_count) == aiReturn_SUCCESS &&
        shininess > 0.0f)
      roughness = sqrtf(2.0f / (shininess + 2.0f));
  }

  BLB_Material_SetBaseColor(material, BLB_ColorFromNormalized(color.r, color.g, color.b, fminf(1.0f, fmaxf(0.0f, opacity))));
  BLB_Material_SetPBR(material, fminf(1.0f, fmaxf(0.0f, metallic)), fminf(1.0f, fmaxf(0.04f, roughness)));
  struct aiColor4D imported_specular = {0.0f, 0.0f, 0.0f, 1.0f};

  if (aiGetMaterialColor(source, AI_MATKEY_COLOR_SPECULAR, &imported_specular) == aiReturn_SUCCESS)
    BLB_Material_SetSpecular(material, 1.0f,
                             BLB_ColorFromNormalized(fminf(1.0f, fmaxf(0.0f, imported_specular.r)),
                                                     fminf(1.0f, fmaxf(0.0f, imported_specular.g)),
                                                     fminf(1.0f, fmaxf(0.0f, imported_specular.b)), 1.0f));
  float imported_ior = 1.5f;
  value_count = 1;

  if (aiGetMaterialFloatArray(source, AI_MATKEY_REFRACTI, &imported_ior, &value_count) == aiReturn_SUCCESS &&
      isfinite(imported_ior) && imported_ior > 0.0f)
    BLB_Material_SetIOR(material, imported_ior);

  if (opacity < 0.999f)
    BLB_Material_SetAlphaMode(material, BLB_ALPHA_BLEND);

  struct aiColor4D emissive = {0.0f, 0.0f, 0.0f, 1.0f};

  if (aiGetMaterialColor(source, AI_MATKEY_COLOR_EMISSIVE, &emissive) == aiReturn_SUCCESS) {
    const float strength = fmaxf(emissive.r, fmaxf(emissive.g, emissive.b));

    if (strength > 0.0f)
      BLB_Material_SetEmission(material, BLB_ColorFromNormalized(emissive.r, emissive.g, emissive.b, 1.0f), strength);
  }

  enum aiTextureMapMode map_modes[3] = {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
  unsigned int uv_index = 0;
  BLB_Texture *texture = load_first_material_texture(scene, source, model_path, aiTextureType_BASE_COLOR,
                                                     aiTextureType_DIFFUSE, map_modes, &uv_index);

  if (texture) {
    set_material_texture(material, &material->base_color_texture, texture, uv_index, map_modes);
    BLB_TextureProjection projection = BLB_TEXMAP_UV;

    if (!mesh_has_uvs)
      projection = fallback_mapping == BLB_TEXMAP_UV ? BLB_TEXMAP_STRETCH : fallback_mapping;
    BLB_MaterialTexture_SetProjection(&material->base_color_texture, projection);

    if (!mesh_has_uvs && BLB_DEBUG)
      fprintf(stderr,
              "[BLB:ML_Assimp] Model '%s' has a texture but no UV channel; using "
              "generated projection %d\n",
              model_path ? model_path : "<unknown>", (int)projection);
  }

  map_modes[0] = aiTextureMapMode_Wrap;
  map_modes[1] = aiTextureMapMode_Wrap;
  uv_index = 0;
  texture = load_first_material_texture(scene, source, model_path, aiTextureType_METALNESS,
                                        aiTextureType_DIFFUSE_ROUGHNESS, map_modes, &uv_index);

  if (texture)
    set_material_texture(material, &material->metallic_roughness_texture, texture, uv_index, map_modes);

  map_modes[0] = aiTextureMapMode_Wrap;
  map_modes[1] = aiTextureMapMode_Wrap;
  uv_index = 0;
  texture = load_first_material_texture(scene, source, model_path, aiTextureType_NORMALS, aiTextureType_HEIGHT,
                                        map_modes, &uv_index);

  if (texture)
    set_material_texture(material, &material->normal_texture, texture, uv_index, map_modes);

  map_modes[0] = aiTextureMapMode_Wrap;
  map_modes[1] = aiTextureMapMode_Wrap;
  uv_index = 0;
  texture = load_first_material_texture(scene, source, model_path, aiTextureType_AMBIENT_OCCLUSION,
                                        aiTextureType_LIGHTMAP, map_modes, &uv_index);

  if (texture)
    set_material_texture(material, &material->occlusion_texture, texture, uv_index, map_modes);

  map_modes[0] = aiTextureMapMode_Wrap;
  map_modes[1] = aiTextureMapMode_Wrap;
  uv_index = 0;
  texture = load_first_material_texture(scene, source, model_path, aiTextureType_EMISSIVE, aiTextureType_NONE,
                                        map_modes, &uv_index);

  if (texture)
    set_material_texture(material, &material->emission_texture, texture, uv_index, map_modes);

  if (material->base_color_texture.texture)
    BLB_MaterialTexture_SetSampler(&material->base_color_texture, material->base_color_texture.wrap_u,
                                   material->base_color_texture.wrap_v, BLB_TEXTURE_FILTER_LINEAR,
                                   BLB_TEXTURE_FILTER_LINEAR);

  if (fallback_mapping != BLB_TEXMAP_UV && !material->base_color_texture.texture)
    BLB_MaterialTexture_SetProjection(&material->base_color_texture, fallback_mapping);

  return material;
}

static BLB_Polygon3D *import_mesh(const struct aiMesh *mesh) {
  if (!mesh || !mesh->mVertices || mesh->mNumVertices == 0 || mesh->mNumVertices > UINT_MAX ||
      (mesh->mNumFaces > 0 && !mesh->mFaces) || SIZE_MAX / (size_t)mesh->mNumVertices < sizeof(BLB_Vec3))
    return NULL;

  size_t index_count = 0;

  for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
    const struct aiFace *face = &mesh->mFaces[i];

    if (face->mNumIndices < 3)
      continue;

    if (!face->mIndices)
      return NULL;

    if (face->mNumIndices - 2u > (SIZE_MAX - index_count) / 3u)
      return NULL;
    index_count += (size_t)(face->mNumIndices - 2u) * 3u;
  }

  if (index_count < 3 || index_count > UINT_MAX || index_count > SIZE_MAX / sizeof(unsigned int))
    return NULL;

  BLB_Polygon3D *polygon = calloc(1, sizeof(*polygon));

  if (!polygon)
    return NULL;

  polygon->vertices = calloc(mesh->mNumVertices, sizeof(*polygon->vertices));
  polygon->base_vertices = calloc(mesh->mNumVertices, sizeof(*polygon->base_vertices));
  polygon->normals = calloc(mesh->mNumVertices, sizeof(*polygon->normals));
  polygon->uvs = calloc(mesh->mNumVertices, sizeof(*polygon->uvs));
  polygon->indices = malloc(index_count * sizeof(*polygon->indices));

  if (!polygon->vertices || !polygon->base_vertices || !polygon->normals || !polygon->uvs || !polygon->indices) {
    BLB_ModelFreePolygon(polygon);

    return NULL;
  }

  polygon->vertex_count = mesh->mNumVertices;
  polygon->index_count = index_count;
  polygon->geometry_hash = 0;

  for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
    const struct aiVector3D *position = &mesh->mVertices[i];
    BLB_Vec3 vertex = BLB_V3(position->x, position->y, position->z);

    if (!isfinite(vertex.x) || !isfinite(vertex.y) || !isfinite(vertex.z)) {
      BLB_ModelFreePolygon(polygon);

      return NULL;
    }
    polygon->vertices[i] = vertex;
    polygon->base_vertices[i] = vertex;

    if (mesh->mNormals) {
      const struct aiVector3D *normal = &mesh->mNormals[i];
      BLB_Vec3 normal_value = BLB_V3(normal->x, normal->y, normal->z);
      polygon->normals[i] = isfinite(normal_value.x) && isfinite(normal_value.y) && isfinite(normal_value.z) &&
                                    BLB_LenV3(normal_value) > 0.000001f
                                ? BLB_NormV3(normal_value)
                                : BLB_V3(0.0f, 1.0f, 0.0f);
    } else {
      polygon->normals[i] = BLB_V3(0.0f, 1.0f, 0.0f);
    }

    if (mesh->mTextureCoords[0]) {
      float u = mesh->mTextureCoords[0][i].x;
      float v = mesh->mTextureCoords[0][i].y;
      polygon->uvs[i] = isfinite(u) && isfinite(v) ? BLB_V2(u, v) : BLB_V2(0.0f, 0.0f);
    } else {
      polygon->uvs[i] = BLB_V2(0.0f, 0.0f);
    }
  }

  size_t cursor = 0;

  for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
    const struct aiFace *face = &mesh->mFaces[i];

    if (face->mNumIndices < 3)
      continue;

    if (!face->mIndices) {
      BLB_ModelFreePolygon(polygon);

      return NULL;
    }

    for (unsigned int tri = 1; tri + 1u < face->mNumIndices; ++tri) {
      const unsigned int a = face->mIndices[0];
      const unsigned int b = face->mIndices[tri];
      const unsigned int c = face->mIndices[tri + 1u];

      if (a >= mesh->mNumVertices || b >= mesh->mNumVertices || c >= mesh->mNumVertices) {
        BLB_ModelFreePolygon(polygon);

        return NULL;
      }
      polygon->indices[cursor++] = a;
      polygon->indices[cursor++] = b;
      polygon->indices[cursor++] = c;
    }
  }

  return polygon;
}

static bool build_combined_polygon(BLB_Object3D *object, const char *path) {
  if (!object || !object->submeshes || object->submesh_count == 0)
    return false;

  size_t vertices_count = 0;
  size_t indices_count = 0;

  for (size_t i = 0; i < object->submesh_count; ++i) {
    const BLB_Polygon3D *part = object->submeshes[i].polygon;

    if (!part || !add_size(vertices_count, part->vertex_count, &vertices_count) ||
        !add_size(indices_count, part->index_count, &indices_count))
      return false;
  }

  if (vertices_count == 0 || indices_count < 3 || vertices_count > UINT_MAX || indices_count > UINT_MAX)
    return false;

  BLB_Polygon3D *combined = calloc(1, sizeof(*combined));

  if (!combined)
    return false;

  combined->vertices = malloc(vertices_count * sizeof(*combined->vertices));
  combined->base_vertices = malloc(vertices_count * sizeof(*combined->base_vertices));
  combined->normals = malloc(vertices_count * sizeof(*combined->normals));
  combined->uvs = malloc(vertices_count * sizeof(*combined->uvs));
  combined->indices = malloc(indices_count * sizeof(*combined->indices));

  if (!combined->vertices || !combined->base_vertices || !combined->normals || !combined->uvs || !combined->indices) {
    BLB_ModelFreePolygon(combined);

    return false;
  }

  size_t vertex_offset = 0;
  size_t index_offset = 0;

  for (size_t i = 0; i < object->submesh_count; ++i) {
    const BLB_Polygon3D *part = object->submeshes[i].polygon;
    memcpy(combined->vertices + vertex_offset, part->vertices, part->vertex_count * sizeof(*part->vertices));
    memcpy(combined->base_vertices + vertex_offset, part->base_vertices,
           part->vertex_count * sizeof(*part->base_vertices));
    memcpy(combined->normals + vertex_offset, part->normals, part->vertex_count * sizeof(*part->normals));
    memcpy(combined->uvs + vertex_offset, part->uvs, part->vertex_count * sizeof(*part->uvs));

    for (size_t j = 0; j < part->index_count; ++j)
      combined->indices[index_offset + j] = (unsigned int)(vertex_offset + part->indices[j]);

    vertex_offset += part->vertex_count;
    index_offset += part->index_count;
  }

  combined->vertex_count = vertices_count;
  combined->index_count = indices_count;
  combined->geometry_hash = 0;
  combined->normal = BLB_V3(0.0f, 1.0f, 0.0f);
  object->polygon = combined;
  object->mesh.vertices = combined->vertices;
  object->mesh.normals = combined->normals;
  object->mesh.uvs = combined->uvs;
  object->mesh.vertex_count = combined->vertex_count;
  object->mesh.indices = combined->indices;
  object->mesh.index_count = combined->index_count;
  object->geometry_id = BLB_ModelHashPath(path);

  return true;
}

BLB_Object3D *BLB_LoadModelImported(const char *path, BLB_Vec3 scale, BLB_Vec3 position, BLB_Texture *texture,
                                    BLB_TextureProjection mapping) {
  if (!path || !path[0])
    return NULL;

  BLB_ImportedModelAsset *cached_asset = imported_asset_find(path);

  if (cached_asset)
    return instantiate_imported_asset(cached_asset, scale, position, texture, mapping);

  const char *extension = strrchr(path, '.');
  bool is_fbx = false;

  if (extension && strlen(extension) == 4u) {
    char a = (char)tolower((unsigned char)extension[1]);
    char b = (char)tolower((unsigned char)extension[2]);
    char c = (char)tolower((unsigned char)extension[3]);
    is_fbx = a == 'f' && b == 'b' && c == 'x';
  }
  const unsigned int base_flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FixInfacingNormals |
                                  aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
                                  aiProcess_ValidateDataStructure | aiProcess_SortByPType;

  /* Assimp normalizes glTF UVs to its bottom-left convention too. The engine's
   * PNG uploads use top-left rows, so every imported format needs the V flip. */
  const unsigned int flags = base_flags | (is_fbx ? aiProcess_PopulateArmatureData : 0u) | aiProcess_FlipUVs;
  const struct aiScene *scene = aiImportFile(path, flags);

  if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mNumMeshes == 0) {
    fprintf(stderr, "[BLB:ML_Assimp] Failed to load model '%s': %s\n", path, aiGetErrorString());

    if (scene)
      aiReleaseImport(scene);

    return NULL;
  }

  bool needs_animation = scene_has_skin(scene);

  if (!needs_animation) {
    const struct aiScene *transformed_scene = aiImportFile(path, flags | aiProcess_PreTransformVertices);

    if (transformed_scene && !(transformed_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) &&
        transformed_scene->mNumMeshes > 0) {
      aiReleaseImport(scene);
      scene = transformed_scene;
    } else {
      if (BLB_DEBUG)
        fprintf(stderr,
                "[BLB:ML_Assimp] Warning: pre-transform import failed for '%s'; using "
                "source mesh coordinates: %s\n",
                path, aiGetErrorString());

      if (transformed_scene)
        aiReleaseImport(transformed_scene);
    }
  }

  int *mesh_node_indices = NULL;
  BLB_ModelAnimationAsset *animation_asset = NULL;

  if (needs_animation) {
    animation_asset = build_animation_asset(scene, &mesh_node_indices, &needs_animation, is_fbx);

    if (!animation_asset && BLB_DEBUG) {
      fprintf(stderr, "[BLB:ML_Assimp] Failed to build model hierarchy '%s'\n", path);
      free(mesh_node_indices);
      aiReleaseImport(scene);

      return NULL;
    }
  }

  BLB_Object3D *object = calloc(1, sizeof(*object));

  if (!object)
    goto fail;

  object->type = BLB_OBJECT_CUSTOM_MODEL;

  if (!BLB_OBJECTS_ID || object->type >= BLB_OBJECTS_ID_COUNT) {
    free(object);
    object = NULL;
    goto fail;
  }

  object->id = &BLB_OBJECTS_ID[object->type];

  if (object->id->id == BLB_INVALID_OBJECT_ID) {
    free(object);
    object = NULL;
    goto fail;
  }
  ++object->id->id;

  object->delta_time = BLB_DeltaTime_Get();
  object->rotation = BLB_V3(0.0f, 0.0f, 0.0f);
  object->position = position;
  object->scale = scale;
  object->render_mode = BLB_RENDER_OPAQUE;
  object->color = BLB_COLOR_WHITE;
  object->visible = true;
  object->entity_id = BLB_INVALID_ENTITY_ID;
  object->component_mask = BLB_COMPONENT_TRANSFORM | BLB_COMPONENT_RENDERABLE;

  for (int i = 0; i < BLB_TRANSFORM_TYPE_COUNT; i++) {
    object->physics_sync_mode[i] = BLB_PHYSICS_SYNC_BIDIRECTIONAL;
  }

  object->submeshes = calloc(scene->mNumMeshes, sizeof(*object->submeshes));

  if (!object->submeshes)
    goto fail;

  for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
    const struct aiMesh *source_mesh = scene->mMeshes[i];

    if (!source_mesh || source_mesh->mNumVertices == 0)
      continue;

    int mesh_node_index = mesh_node_indices ? mesh_node_indices[i] : -1;
    BLB_Polygon3D *polygon = import_mesh(source_mesh);

    if (!polygon)
      continue;

    const bool mesh_has_uvs = source_mesh->mTextureCoords[0] != NULL;

    BLB_Material *material = NULL;

    if (source_mesh->mMaterialIndex < scene->mNumMaterials && scene->mMaterials[source_mesh->mMaterialIndex])
      material = import_material(scene, scene->mMaterials[source_mesh->mMaterialIndex], path, mapping, mesh_has_uvs);
    else {
      if (BLB_DEBUG)
        fprintf(stderr,
                "[BLB:ML_Assimp] Model '%s' mesh %u has no valid material; using a "
                "default material\n",
                path, i);
      material = BLB_Material_Create3D();
    }

    if (!material) {
      BLB_ModelFreePolygon(polygon);
      goto fail;
    }

    if (needs_animation && !import_skin_submesh(animation_asset, source_mesh, mesh_node_index, is_fbx)) {
      BLB_ModelFreePolygon(polygon);
      BLB_Material_Release(material);
      goto fail;
    }

    BLB_ObjectSubmesh *submesh = &object->submeshes[object->submesh_count++];
    submesh->polygon = polygon;
    submesh->material = material;
  }

  if (object->submesh_count == 0 || !build_combined_polygon(object, path))
    goto fail;

  object->material = object->submeshes[0].material;
  BLB_Material_Retain(object->material);
  BLB_Object3D_Transform(object, position, object->rotation, scale);

  free(mesh_node_indices);
  mesh_node_indices = NULL;

  if (animation_asset) {
    for (size_t i = 0; i < animation_asset->node_count; ++i)
      animation_asset->nodes[i].source_node = NULL;
  }

  aiReleaseImport(scene);
  scene = NULL;

  if (!needs_animation && animation_asset) {
    BLB_ModelAnimationAsset_Destroy(animation_asset);
    animation_asset = NULL;
  }

  BLB_ImportedModelAsset *asset = imported_asset_take_object(object, path, animation_asset);

  if (!asset) {
    BLB_DestroyModelImported(object);
    BLB_ModelAnimationAsset_Destroy(animation_asset);

    return NULL;
  }

  animation_asset = NULL;
  BLB_DestroyModelImported(object);

  return instantiate_imported_asset(asset, scale, position, texture, mapping);
fail:
  free(mesh_node_indices);

  if (scene)
    aiReleaseImport(scene);

  BLB_ModelAnimationAsset_Destroy(animation_asset);
  BLB_DestroyModelImported(object);

  return NULL;
}

void BLB_DestroyModelImported(BLB_Object3D *object) {
  if (!object)
    return;

  if (object->rigid_body)
    BLB_Physics3D_BodyDestroy(object->rigid_body->world, object);

  BLB_Object3D_ClearLODs(object);
  BLB_ModelAnimationInstance_Destroy(object);

  BLB_ImportedModelAsset *asset = object->model_asset;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  for (size_t i = 0; i < object->submesh_count; ++i) {
    if (!asset || object->owns_model_geometry)
      BLB_ModelFreePolygon(object->submeshes[i].polygon);

    if (object->submeshes[i].material)
      BLB_Material_Release(object->submeshes[i].material);
  }

  free(object->submeshes);

  if (!asset || object->owns_model_geometry)
    BLB_ModelFreePolygon(object->polygon);

  if (object->id && object->id->id > 0)
    --object->id->id;

  free(object->handler);

  if (asset)
    imported_asset_release(asset);

  free(object);
}
