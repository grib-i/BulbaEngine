#include "bulba/core/render.h"
#include "bulba/core/platform.h"
#include "bulba/core/render/render_debug.h"
#include "bulba/core/utils/delta_time.h"

#include "bulba/core/math3v/lights.h"
#include "bulba/core/math3v/math3v.h"
#include "bulba/core/math3v/vec_math.h"
#include "bulba/core/objects3d/animation3d.h"
#include "bulba/core/objects3d/audio3d.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/physics/physics2d.h"
#include "bulba/core/physics/physics3d.h"
#include "bulba/core/render/object_lod.h"
#include "bulba/core/render/optimization/object_aggregation.h"
#include "bulba/core/render/optimization/object_optimization2d.h"
#include "bulba/core/render/optimization/render_async.h"
#include "bulba/core/render/optimization/visibility_cache.h"
#include "bulba/core/utils/config.h"
#include "bulba/core/utils/fps.h"
#include "bulba/core/utils/object.h"
#include "bulba/graphics/renderer.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef BLB_GLOW_STEPS_3D
#define BLB_GLOW_STEPS_3D 8
#endif

#ifndef BLB_GLOW_STEPS_2D
#define BLB_GLOW_STEPS_2D 8
#endif

#ifndef BLB_OBJECT2D_CAMERA_SPACE_DEPTH
#define BLB_OBJECT2D_CAMERA_SPACE_DEPTH 1.0f
#endif

typedef struct {
  float base_color[4];
  float emission;
  float glow;
  float glow_radius;
  float glow_falloff;
  float roughness;
  float metallic;
  float normal_scale;
  float specular;
  float occlusion;
  float specular_color[3];
  float emission_color[4];
  float temperature;
  float ior;
  float transmission;
  float volume_thickness;
  float attenuation_color[3];
  float attenuation_distance;
  float clearcoat_factor;
  float clearcoat_roughness;
  float clearcoat_normal_scale;
  float sheen_color[3];
  float sheen_roughness;
  float iridescence_factor;
  float iridescence_ior;
  float iridescence_thickness_min;
  float iridescence_thickness_max;
  float anisotropy_strength;
  float anisotropy_rotation;
  float dispersion;
  float entity_id;
  float alpha_cutoff;
  BLB_AlphaMode alpha_mode;
  bool lighting_enabled;
  bool depth_enabled;
  bool depth_write;
  bool double_sided;
  bool unlit;
  const BLB_Material *source_material;
  BLB_RenderMode render_mode;
} BLB_RenderMaterialState;

static void debug_visibility(BLB_RenderVisibilityCache *cache) {
  if (!cache || !BLB_DEBUG)
    return;

  uint64_t hits = 0;
  uint64_t misses = 0;
  size_t entries = 0;

  BLB_RenderVisibilityCache_GetStats(cache, &hits, &misses, &entries);

  fprintf(stderr,
          "[BLB:Visibility] cache=ON hits=%llu misses=%llu entries=%zu "
          "camera_revision=%llu\n",
          (unsigned long long)hits, (unsigned long long)misses, entries,
          (unsigned long long)BLB_RenderVisibilityCache_GetCameraRevision(cache));
}

static void rgb(const BLB_Vec4 color, float output[4]) {
  output[0] = color.x / 255.0f;
  output[1] = color.y / 255.0f;
  output[2] = color.z / 255.0f;
  output[3] = color.w / 255.0f;
}

static BLB_RenderMode normalize_render_mode(BLB_RenderMode mode) {
  if ((int)mode < (int)BLB_RENDER_OPAQUE || (int)mode >= (int)BLB_RENDER_MODE_COUNT)
    return BLB_RENDER_OPAQUE;

  return mode;
}

static int compare_int(int a, int b) {
  if (a < b)
    return -1;

  if (a > b)
    return 1;

  return 0;
}

static BLB_RenderMode object3d_render_mode(const BLB_Object3D *object) {
  if (!object)
    return BLB_RENDER_OPAQUE;

  if (object->material) {
    BLB_RenderMode mode = normalize_render_mode(object->material->render_mode);

    return mode == BLB_RENDER_OPAQUE && object->material->alpha_mode == BLB_ALPHA_BLEND ? BLB_RENDER_TRANSPARENT : mode;
  }

  return normalize_render_mode(object->render_mode);
}

static BLB_RenderMode object2d_render_mode(const BLB_Object2D *object) {
  if (!object)
    return BLB_RENDER_OPAQUE;

  if (object->material) {
    BLB_RenderMode mode = normalize_render_mode(object->material->render_mode);

    return mode == BLB_RENDER_OPAQUE && object->material->alpha_mode == BLB_ALPHA_BLEND ? BLB_RENDER_TRANSPARENT : mode;
  }

  return normalize_render_mode(object->render_mode);
}

static BLB_RenderMode text2d_render_mode(const BLB_Text2D *text) {
  if (!text)
    return BLB_RENDER_OPAQUE;

  if (text->material)
    return normalize_render_mode(text->material->render_mode);

  return normalize_render_mode(text->render_mode);
}

static int compare_object3d(const void *a, const void *b) {
  const BLB_Object3D *aa = *(BLB_Object3D *const *)a;
  const BLB_Object3D *bb = *(BLB_Object3D *const *)b;

  if (!aa || !bb) {
    if (aa)
      return -1;

    if (bb)
      return 1;

    return 0;
  }

  BLB_RenderMode am = object3d_render_mode(aa);
  BLB_RenderMode bm = object3d_render_mode(bb);

  if (am != bm)
    return compare_int((int)am, (int)bm);

  return compare_int(aa->layer, bb->layer);
}

static int compare_object2d(const void *a, const void *b) {
  const BLB_Object2D *aa = *(BLB_Object2D *const *)a;
  const BLB_Object2D *bb = *(BLB_Object2D *const *)b;

  if (!aa || !bb) {
    if (aa)
      return -1;

    if (bb)
      return 1;

    return 0;
  }

  BLB_RenderMode am = object2d_render_mode(aa);
  BLB_RenderMode bm = object2d_render_mode(bb);

  if (am != bm)
    return compare_int((int)am, (int)bm);

  return compare_int(aa->layer, bb->layer);
}

static int compare_text2d(const void *a, const void *b) {
  const BLB_Text2D *aa = *(BLB_Text2D *const *)a;
  const BLB_Text2D *bb = *(BLB_Text2D *const *)b;

  if (!aa || !bb) {
    if (aa)
      return -1;

    if (bb)
      return 1;

    return 0;
  }

  BLB_RenderMode am = text2d_render_mode(aa);
  BLB_RenderMode bm = text2d_render_mode(bb);

  if (am != bm)
    return compare_int((int)am, (int)bm);

  return compare_int(aa->layer, bb->layer);
}

static int compare_light3d(const void *a, const void *b) {
  const BLB_Light3D *aa = *(BLB_Light3D *const *)a;
  const BLB_Light3D *bb = *(BLB_Light3D *const *)b;

  if (!aa || !bb) {
    if (aa)
      return -1;

    if (bb)
      return 1;

    return 0;
  }

  if (!aa->object || !bb->object) {
    if (aa->object)
      return -1;

    if (bb->object)
      return 1;

    return 0;
  }

  return compare_object3d(&aa->object, &bb->object);
}

static int compare_light2d(const void *a, const void *b) {
  const BLB_Light2D *aa = *(BLB_Light2D *const *)a;
  const BLB_Light2D *bb = *(BLB_Light2D *const *)b;

  if (!aa || !bb) {
    if (aa)
      return -1;

    if (bb)
      return 1;

    return 0;
  }

  if (!aa->object || !bb->object) {
    if (aa->object)
      return -1;

    if (bb->object)
      return 1;

    return 0;
  }

  return compare_object2d(&aa->object, &bb->object);
}

static void *get_render_object(BLB_Scene *scene, int type, size_t index) {
  if (!scene)
    return NULL;

  switch (type) {
  case 0:
    return index < (size_t)scene->object3d_count ? scene->objects3d[index] : NULL;
  case 1:
    return index < (size_t)scene->object2d_count ? scene->objects2d[index] : NULL;
  case 2:
    return index < (size_t)scene->text2d_count ? scene->text2d[index] : NULL;
  case 3:
    return index < (size_t)scene->light3d_count && scene->lights3d[index] ? scene->lights3d[index]->object : NULL;
  case 4:
    return index < (size_t)scene->light2d_count && scene->lights2d[index] ? scene->lights2d[index]->object : NULL;
  case 5:
    return index < (size_t)scene->audio2d_count && scene->audio2d[index] ? scene->audio2d[index]->obj : NULL;
  case 6:
    return index < (size_t)scene->audio3d_count && scene->audio3d[index] ? scene->audio3d[index]->obj : NULL;
  }

  return NULL;
}

static BLB_RenderMode get_render_mode(void *object, int type) {
  if (!object)
    return BLB_RENDER_OPAQUE;

  if (type == 0 || type == 3 || type == 6)
    return object3d_render_mode((BLB_Object3D *)object);

  if (type == 1 || type == 4 || type == 5)
    return object2d_render_mode((BLB_Object2D *)object);

  if (type == 2)
    return text2d_render_mode((BLB_Text2D *)object);

  return BLB_RENDER_OPAQUE;
}

static int get_render_layer(void *object, int type) {
  if (!object)
    return 0;

  if (type == 0 || type == 3 || type == 6)
    return ((BLB_Object3D *)object)->layer;

  if (type == 1 || type == 4 || type == 5)
    return ((BLB_Object2D *)object)->layer;

  if (type == 2)
    return ((BLB_Text2D *)object)->layer;

  return 0;
}

static BLB_RenderMaterialState material_state_from_3d(const BLB_Object3D *object) {
  BLB_RenderMaterialState state = {0};

  if (!object)
    return state;

  if (object->material) {
    const BLB_Material *material = object->material;

    state.base_color[0] = material->base_color[0] * object->color.x / 255.0f;
    state.base_color[1] = material->base_color[1] * object->color.y / 255.0f;
    state.base_color[2] = material->base_color[2] * object->color.z / 255.0f;
    state.base_color[3] = material->base_color[3] * object->color.w / 255.0f;

    state.emission = material->emission_strength;
    state.glow = material->glow_strength;
    state.glow_radius = material->glow_radius;
    state.glow_falloff = material->glow_falloff;
    state.roughness = material->roughness;
    state.metallic = material->metallic;
    state.specular = material->specular_factor;
    state.occlusion = material->occlusion_strength;
    state.specular_color[0] = material->specular_color[0];
    state.specular_color[1] = material->specular_color[1];
    state.specular_color[2] = material->specular_color[2];
    state.emission_color[0] = material->emission_color[0];
    state.emission_color[1] = material->emission_color[1];
    state.emission_color[2] = material->emission_color[2];
    state.emission_color[3] = material->emission_color[3];
    state.temperature = material->temperature;
    state.normal_scale = material->normal_scale;
    state.ior = material->ior;
    state.transmission = material->transmission;
    state.volume_thickness = material->volume_thickness;
    state.attenuation_color[0] = material->attenuation_color[0];
    state.attenuation_color[1] = material->attenuation_color[1];
    state.attenuation_color[2] = material->attenuation_color[2];
    state.attenuation_distance = material->attenuation_distance;
    state.clearcoat_factor = material->clearcoat_factor;
    state.clearcoat_roughness = material->clearcoat_roughness;
    state.clearcoat_normal_scale = material->clearcoat_normal_scale;
    state.sheen_color[0] = material->sheen_color[0];
    state.sheen_color[1] = material->sheen_color[1];
    state.sheen_color[2] = material->sheen_color[2];
    state.sheen_roughness = material->sheen_roughness;
    state.iridescence_factor = material->iridescence_factor;
    state.iridescence_ior = material->iridescence_ior;
    state.iridescence_thickness_min = material->iridescence_thickness_min;
    state.iridescence_thickness_max = material->iridescence_thickness_max;
    state.anisotropy_strength = material->anisotropy_strength;
    state.anisotropy_rotation = material->anisotropy_rotation;
    state.dispersion = material->dispersion;
    state.entity_id = (float)object->entity_id;
    state.alpha_cutoff = material->alpha_cutoff;
    state.alpha_mode = material->alpha_mode;
    state.lighting_enabled = material->lighting_enabled;
    state.depth_enabled = material->depth_enabled;
    state.depth_write = material->depth_write;
    state.double_sided = material->double_sided;
    state.unlit = material->unlit;
    state.render_mode = normalize_render_mode(material->render_mode);
    state.source_material = material;

    return state;
  }

  state.render_mode = normalize_render_mode(object->render_mode);
  state.lighting_enabled = true;
  state.depth_enabled = true;
  state.depth_write = true;
  state.double_sided = false;
  state.unlit = false;
  rgb(object->color, state.base_color);
  state.emission = 0.0f;
  state.glow = 0.0f;
  state.glow_radius = 0.0f;
  state.glow_falloff = 2.0f;
  state.roughness = object->roundness;
  state.metallic = 0.0f;
  state.specular = 1.0f;
  state.occlusion = 1.0f;
  state.specular_color[0] = 1.0f;
  state.specular_color[1] = 1.0f;
  state.specular_color[2] = 1.0f;
  state.emission_color[0] = 1.0f;
  state.emission_color[1] = 1.0f;
  state.emission_color[2] = 1.0f;
  state.emission_color[3] = 1.0f;
  state.temperature = 6500.0f;
  state.entity_id = (float)object->entity_id;
  state.alpha_cutoff = 0.5f;
  state.alpha_mode = BLB_ALPHA_OPAQUE;
  state.normal_scale = 1.0f;
  state.ior = 1.5f;
  state.transmission = 0.0f;
  state.volume_thickness = 0.0f;
  state.attenuation_color[0] = 1.0f;
  state.attenuation_color[1] = 1.0f;
  state.attenuation_color[2] = 1.0f;
  state.attenuation_distance = INFINITY;
  state.clearcoat_normal_scale = 1.0f;
  state.iridescence_ior = 1.3f;
  state.iridescence_thickness_min = 100.0f;
  state.iridescence_thickness_max = 400.0f;

  return state;
}

static BLB_RenderMaterialState material_state_from_2d(const BLB_Object2D *object) {
  BLB_RenderMaterialState state = {0};

  if (!object)
    return state;

  if (object->material) {
    const BLB_Material *material = object->material;

    state.base_color[0] = material->base_color[0] * object->color.x / 255.0f;
    state.base_color[1] = material->base_color[1] * object->color.y / 255.0f;
    state.base_color[2] = material->base_color[2] * object->color.z / 255.0f;
    state.base_color[3] = material->base_color[3] * object->color.w / 255.0f;
    state.emission = material->emission_strength;
    state.glow = material->glow_strength;
    state.glow_radius = material->glow_radius;
    state.glow_falloff = material->glow_falloff;
    state.roughness = material->roughness;
    state.metallic = material->metallic;
    state.specular = material->specular_factor;
    state.occlusion = material->occlusion_strength;
    state.specular_color[0] = material->specular_color[0];
    state.specular_color[1] = material->specular_color[1];
    state.specular_color[2] = material->specular_color[2];
    state.emission_color[0] = material->emission_color[0];
    state.emission_color[1] = material->emission_color[1];
    state.emission_color[2] = material->emission_color[2];
    state.emission_color[3] = material->emission_color[3];
    state.temperature = material->temperature;
    state.normal_scale = material->normal_scale;
    state.ior = material->ior;
    state.transmission = material->transmission;
    state.volume_thickness = material->volume_thickness;
    state.attenuation_color[0] = material->attenuation_color[0];
    state.attenuation_color[1] = material->attenuation_color[1];
    state.attenuation_color[2] = material->attenuation_color[2];
    state.attenuation_distance = material->attenuation_distance;
    state.clearcoat_factor = material->clearcoat_factor;
    state.clearcoat_roughness = material->clearcoat_roughness;
    state.clearcoat_normal_scale = material->clearcoat_normal_scale;
    state.sheen_color[0] = material->sheen_color[0];
    state.sheen_color[1] = material->sheen_color[1];
    state.sheen_color[2] = material->sheen_color[2];
    state.sheen_roughness = material->sheen_roughness;
    state.iridescence_factor = material->iridescence_factor;
    state.iridescence_ior = material->iridescence_ior;
    state.iridescence_thickness_min = material->iridescence_thickness_min;
    state.iridescence_thickness_max = material->iridescence_thickness_max;
    state.anisotropy_strength = material->anisotropy_strength;
    state.anisotropy_rotation = material->anisotropy_rotation;
    state.dispersion = material->dispersion;
    state.entity_id = (float)object->entity_id;
    state.alpha_cutoff = material->alpha_cutoff;
    state.alpha_mode = material->alpha_mode;
    state.lighting_enabled = material->lighting_enabled;
    state.depth_enabled = material->depth_enabled;
    state.depth_write = material->depth_write;
    state.double_sided = material->double_sided;
    state.unlit = material->unlit;
    state.render_mode = normalize_render_mode(material->render_mode);
    state.source_material = material;

    return state;
  }

  state.render_mode = normalize_render_mode(object->render_mode);
  state.lighting_enabled = false;
  state.depth_enabled = false;
  state.depth_write = false;
  state.double_sided = true;
  state.unlit = true;
  rgb(object->color, state.base_color);
  state.emission = 0.0f;
  state.glow = 0.0f;
  state.glow_radius = 0.0f;
  state.glow_falloff = 2.0f;
  state.roughness = object->roundness;
  state.metallic = 0.0f;
  state.specular = 1.0f;
  state.occlusion = 1.0f;
  state.specular_color[0] = 1.0f;
  state.specular_color[1] = 1.0f;
  state.specular_color[2] = 1.0f;
  state.emission_color[0] = 1.0f;
  state.emission_color[1] = 1.0f;
  state.emission_color[2] = 1.0f;
  state.emission_color[3] = 1.0f;
  state.temperature = 6500.0f;
  state.entity_id = (float)object->entity_id;
  state.alpha_cutoff = 0.5f;
  state.alpha_mode = BLB_ALPHA_OPAQUE;
  state.normal_scale = 1.0f;
  state.ior = 1.5f;
  state.transmission = 0.0f;
  state.volume_thickness = 0.0f;
  state.attenuation_color[0] = 1.0f;
  state.attenuation_color[1] = 1.0f;
  state.attenuation_color[2] = 1.0f;
  state.attenuation_distance = INFINITY;
  state.clearcoat_normal_scale = 1.0f;
  state.iridescence_ior = 1.3f;
  state.iridescence_thickness_min = 100.0f;
  state.iridescence_thickness_max = 400.0f;

  return state;
}

static BLB_RenderMaterialState material_state_from_text(const BLB_Text2D *text) {
  BLB_RenderMaterialState state = {0};

  if (!text)
    return state;

  if (text->material) {
    const BLB_Material *material = text->material;
    printf("32");

    state.base_color[0] = material->base_color[0];
    state.base_color[1] = material->base_color[1];
    state.base_color[2] = material->base_color[2];
    state.base_color[3] = material->base_color[3];
    state.emission = material->emission_strength;
    state.glow = material->glow_strength;
    state.glow_radius = material->glow_radius;
    state.glow_falloff = material->glow_falloff;
    state.roughness = material->roughness;
    state.metallic = material->metallic;
    state.specular = material->specular_factor;
    state.occlusion = material->occlusion_strength;
    state.specular_color[0] = material->specular_color[0];
    state.specular_color[1] = material->specular_color[1];
    state.specular_color[2] = material->specular_color[2];
    state.emission_color[0] = material->emission_color[0];
    state.emission_color[1] = material->emission_color[1];
    state.emission_color[2] = material->emission_color[2];
    state.emission_color[3] = material->emission_color[3];
    state.temperature = material->temperature;
    state.ior = material->ior;
    state.transmission = material->transmission;
    state.volume_thickness = material->volume_thickness;
    state.attenuation_color[0] = material->attenuation_color[0];
    state.attenuation_color[1] = material->attenuation_color[1];
    state.attenuation_color[2] = material->attenuation_color[2];
    state.attenuation_distance = material->attenuation_distance;
    state.clearcoat_factor = material->clearcoat_factor;
    state.clearcoat_roughness = material->clearcoat_roughness;
    state.clearcoat_normal_scale = material->clearcoat_normal_scale;
    state.sheen_color[0] = material->sheen_color[0];
    state.sheen_color[1] = material->sheen_color[1];
    state.sheen_color[2] = material->sheen_color[2];
    state.sheen_roughness = material->sheen_roughness;
    state.iridescence_factor = material->iridescence_factor;
    state.iridescence_ior = material->iridescence_ior;
    state.iridescence_thickness_min = material->iridescence_thickness_min;
    state.iridescence_thickness_max = material->iridescence_thickness_max;
    state.anisotropy_strength = material->anisotropy_strength;
    state.anisotropy_rotation = material->anisotropy_rotation;
    state.dispersion = material->dispersion;
    state.entity_id = (float)text->entity_id;
    state.alpha_cutoff = material->alpha_cutoff;
    state.alpha_mode = material->alpha_mode;
    state.lighting_enabled = false;
    state.depth_enabled = material->depth_enabled;
    state.depth_write = material->depth_write;
    state.double_sided = material->double_sided;
    state.unlit = true;
    state.render_mode = normalize_render_mode(material->render_mode);
    state.source_material = material;

    return state;
  }

  state.base_color[0] = text->color[0] / 255.0f;
  state.base_color[1] = text->color[1] / 255.0f;
  state.base_color[2] = text->color[2] / 255.0f;
  state.base_color[3] = text->color[3] / 255.0f;
  state.emission = text->emission;
  state.glow = text->glow;
  state.roughness = text->roundness;
  state.lighting_enabled = false;
  state.depth_enabled = false;
  state.depth_write = false;
  state.double_sided = true;
  state.unlit = true;
  state.render_mode = normalize_render_mode(text->render_mode);

  return state;
}

static void build_model(BLB_Vec3 position, BLB_Vec3 rotation, BLB_Vec3 scale, BLB_Mat4 *model) {
  *model = BLB_M4D(1.0f);
  if (rotation.x == 0.0f && rotation.y == 0.0f && rotation.z == 0.0f) {
    model->Elements[0][0] = scale.x;
    model->Elements[1][1] = scale.y;
    model->Elements[2][2] = scale.z;
  } else {
    float x = rotation.x * (float)BLB_PI / 180.0f;
    float y = rotation.y * (float)BLB_PI / 180.0f;
    float z = rotation.z * (float)BLB_PI / 180.0f;
    float sx = sinf(x), cx = cosf(x), sy = sinf(y), cy = cosf(y), sz = sinf(z), cz = cosf(z);
    model->Elements[0][0] = cz * cy * scale.x;
    model->Elements[0][1] = sz * cy * scale.x;
    model->Elements[0][2] = -sy * scale.x;
    model->Elements[1][0] = (cz * sy * sx - sz * cx) * scale.y;
    model->Elements[1][1] = (sz * sy * sx + cz * cx) * scale.y;
    model->Elements[1][2] = cy * sx * scale.y;
    model->Elements[2][0] = (cz * sy * cx + sz * sx) * scale.z;
    model->Elements[2][1] = (sz * sy * cx - cz * sx) * scale.z;
    model->Elements[2][2] = cy * cx * scale.z;
  }
  model->Elements[3][0] = position.x;
  model->Elements[3][1] = position.y;
  model->Elements[3][2] = position.z;
}

BLB_Mat4 blb_render_object3d_model_matrix(const BLB_Object3D *object, const BLB_CameraCache *camera_cache,
                                          float scale_mul) {
  BLB_Mat4 model = BLB_M4D(1.0f);

  if (!object)
    return model;

  build_model(object->position, object->rotation, BLB_MulV3F(object->scale, scale_mul), &model);

  if (object->camera_attached && camera_cache && camera_cache->valid) {
    BLB_Mat4 camera_world = BLB_CameraPixelToWorld(camera_cache);
    model = BLB_MulM4(camera_world, model);
  }

  return model;
}

static void extract_matrix_rows(const BLB_Mat4 *matrix, float rows[12]) {
  rows[0] = matrix->Elements[0][0];
  rows[1] = matrix->Elements[1][0];
  rows[2] = matrix->Elements[2][0];
  rows[3] = matrix->Elements[3][0];
  rows[4] = matrix->Elements[0][1];
  rows[5] = matrix->Elements[1][1];
  rows[6] = matrix->Elements[2][1];
  rows[7] = matrix->Elements[3][1];
  rows[8] = matrix->Elements[0][2];
  rows[9] = matrix->Elements[1][2];
  rows[10] = matrix->Elements[2][2];
  rows[11] = matrix->Elements[3][2];
}

static float shared_polygon_radius(const BLB_Polygon3D *polygon) {
  if (!polygon || !polygon->vertices || polygon->vertex_count == 0)
    return 0.0f;

  if (!polygon->dynamic_vertices && polygon->bounds_radius > 0.0f)
    return polygon->bounds_radius;

  float radius_squared = 0.0f;

  for (size_t i = 0; i < polygon->vertex_count; ++i)
    radius_squared = fmaxf(radius_squared, BLB_LenSqrV3(polygon->vertices[i]));

  float radius = sqrtf(radius_squared);

  if (!polygon->dynamic_vertices)
    ((BLB_Polygon3D *)polygon)->bounds_radius = radius;

  return radius;
}

static const BLB_Polygon3D *active_polygon3d(const BLB_Object3D *object) {
  if (!object)
    return NULL;

  if (object->type == BLB_OBJECT_CUSTOM_MODEL && (object->submesh_count > 1 || object->animation3d))
    return object->polygon;

  return BLB_Object3D_GetLODPolygon(object);
}

static const BLB_Polygon2D *active_polygon2d(const BLB_Object2D *object) {
  return BLB_Object2D_GetLODPolygon(object);
}

static float object_local_radius(BLB_Object3D *object) {
  if (!object || !object->polygon || !object->polygon->vertices || object->polygon->vertex_count == 0)
    return 0.0f;

  const BLB_Polygon3D *polygon = object->polygon;

  float radius = shared_polygon_radius(polygon);
  object->bounds_radius = radius;

  return radius;
}

static bool view_space_sphere_visible(const BLB_CameraCache *camera_cache, BLB_Vec3 center, float radius,
                                      float margin) {
  if (!camera_cache || !camera_cache->valid)
    return true;

  radius *= 1.0f + fmaxf(margin, 0.0f);

  if (camera_cache->frustum_valid) {
    for (size_t i = 0; i < 6; ++i) {
      const float *plane = camera_cache->frustum_planes[i];

      if (plane[0] * center.x + plane[1] * center.y + plane[2] * center.z + plane[3] < -radius)
        return false;
    }

    return true;
  }

  const BLB_Mat4 *view = &camera_cache->view;

  const float x = view->Elements[0][0] * center.x + view->Elements[1][0] * center.y + view->Elements[2][0] * center.z +
                  view->Elements[3][0];
  const float y = view->Elements[0][1] * center.x + view->Elements[1][1] * center.y + view->Elements[2][1] * center.z +
                  view->Elements[3][1];
  const float z = view->Elements[0][2] * center.x + view->Elements[1][2] * center.y + view->Elements[2][2] * center.z +
                  view->Elements[3][2];

  const float depth = -z;

  if (depth + radius <= 0.0f)
    return false;

  const float projection_x = fabsf(camera_cache->projection.Elements[0][0]);
  const float projection_y = fabsf(camera_cache->projection.Elements[1][1]);

  if (projection_x <= 0.000001f || projection_y <= 0.000001f)
    return true;

  const float half_width = (depth + radius) / projection_x;
  const float half_height = (depth + radius) / projection_y;

  if (fabsf(x) > half_width + radius)
    return false;

  if (fabsf(y) > half_height + radius)
    return false;

  return true;
}

typedef struct {
  BLB_Vec3 normals[6];
  float offsets[6];
  float lengths[6];
} BLB_ShadowFrustum;

static void shadow_frustum_build(BLB_ShadowFrustum *frustum, const BLB_Mat4 *shadow_vp) {
  if (!frustum || !shadow_vp)
    return;

  // normals and offests 0
  frustum->normals[0] = BLB_V3(shadow_vp->Elements[0][0] + shadow_vp->Elements[0][3],
                               shadow_vp->Elements[1][0] + shadow_vp->Elements[1][3],
                               shadow_vp->Elements[2][0] + shadow_vp->Elements[2][3]);
  frustum->offsets[0] = shadow_vp->Elements[3][0] + shadow_vp->Elements[3][3];

  // normals and offests 1
  frustum->normals[1] = BLB_V3(shadow_vp->Elements[0][3] - shadow_vp->Elements[0][0],
                               shadow_vp->Elements[1][3] - shadow_vp->Elements[1][0],
                               shadow_vp->Elements[2][3] - shadow_vp->Elements[2][0]);
  frustum->offsets[1] = shadow_vp->Elements[3][3] - shadow_vp->Elements[3][0];

  // normals and offests 2
  frustum->normals[2] = BLB_V3(shadow_vp->Elements[0][1] + shadow_vp->Elements[0][3],
                               shadow_vp->Elements[1][1] + shadow_vp->Elements[1][3],
                               shadow_vp->Elements[2][1] + shadow_vp->Elements[2][3]);
  frustum->offsets[2] = shadow_vp->Elements[3][1] + shadow_vp->Elements[3][3];

  // normals and offests 3
  frustum->normals[3] = BLB_V3(shadow_vp->Elements[0][3] - shadow_vp->Elements[0][1],
                               shadow_vp->Elements[1][3] - shadow_vp->Elements[1][1],
                               shadow_vp->Elements[2][3] - shadow_vp->Elements[2][1]);
  frustum->offsets[3] = shadow_vp->Elements[3][3] - shadow_vp->Elements[3][1];

  // normals and offests 4
  frustum->normals[4] = BLB_V3(shadow_vp->Elements[0][2], shadow_vp->Elements[1][2], shadow_vp->Elements[2][2]);
  frustum->offsets[4] = shadow_vp->Elements[3][2];

  // normals and offests 5
  frustum->normals[5] = BLB_V3(shadow_vp->Elements[0][3] - shadow_vp->Elements[0][2],
                               shadow_vp->Elements[1][3] - shadow_vp->Elements[1][2],
                               shadow_vp->Elements[2][3] - shadow_vp->Elements[2][2]);
  frustum->offsets[5] = shadow_vp->Elements[3][3] - shadow_vp->Elements[3][2];

  for (int i = 0; i < 6; ++i)
    frustum->lengths[i] = BLB_LenV3(frustum->normals[i]);
}

static bool shadow_sphere_visible(const BLB_ShadowFrustum *frustum, BLB_Vec3 center, float radius) {
  if (!frustum)
    return true;

  radius = fmaxf(radius, 0.0f);

  for (int i = 0; i < 6; ++i) {
    float distance = BLB_DotV3(frustum->normals[i], center) + frustum->offsets[i];

    if (distance < -radius * frustum->lengths[i])
      return false;
  }

  return true;
}

static bool object_visible_to_camera(BLB_Object3D *object, const BLB_CameraCache *camera_cache,
                                     BLB_Vec3 camera_position) {
  if (!object || !object->visible)
    return false;

  if (object->camera_attached)
    return true;

  if (!BLB_OBJECT_CULLING)
    return true;

  float radius = object_local_radius(object);
  float scale_x = fabsf(object->scale.x);
  float scale_y = fabsf(object->scale.y);
  float scale_z = fabsf(object->scale.z);
  float scale = fmaxf(scale_x, fmaxf(scale_y, scale_z));

  radius *= scale;

  if (BLB_OBJECT_DISTANCE_CULLING && BLB_OBJECT_MAX_RENDER_DISTANCE > 0.0f) {
    BLB_Vec3 delta = BLB_SubV3(object->position, camera_position);
    float max_distance = BLB_OBJECT_MAX_RENDER_DISTANCE + radius;

    if (BLB_DotV3(delta, delta) > max_distance * max_distance)
      return false;
  }

  if (!BLB_OBJECT_FRUSTUM_CULLING || !camera_cache || !camera_cache->valid)
    return true;

  return view_space_sphere_visible(camera_cache, object->position, radius, BLB_OBJECT_CULLING_MARGIN);
}

static bool object2d_visible_to_camera(const BLB_Object2D *object, const BLB_CameraCache *camera_cache,
                                       BLB_Vec3 camera_position, float viewport_width, float viewport_height) {
  const BLB_Polygon2D *polygon = active_polygon2d(object);

  if (!object || !object->visible || !polygon || !polygon->vertices || polygon->vertex_count == 0)
    return false;

  if (!object->camera_attached && !BLB_OBJECT_CULLING)
    return true;

  float radius_squared = 0.0f;

  for (size_t i = 0; i < polygon->vertex_count; ++i)
    radius_squared = fmaxf(radius_squared, BLB_DotV2(polygon->vertices[i], polygon->vertices[i]));
  float radius = sqrtf(radius_squared) * fmaxf(fabsf(object->scale.x), fabsf(object->scale.y));

  if (object->camera_attached)
    return object->position.x + radius >= 0.0f && object->position.x - radius <= viewport_width &&
           object->position.y + radius >= 0.0f && object->position.y - radius <= viewport_height;

  if (BLB_OBJECT_DISTANCE_CULLING && BLB_OBJECT_MAX_RENDER_DISTANCE > 0.0f) {
    float dx = object->position.x - camera_position.x;
    float dy = object->position.y - camera_position.y;
    float max_distance = BLB_OBJECT_MAX_RENDER_DISTANCE + radius;

    if (dx * dx + dy * dy + camera_position.z * camera_position.z > max_distance * max_distance)
      return false;
  }

  if (!BLB_OBJECT_FRUSTUM_CULLING)
    return true;

  return view_space_sphere_visible(camera_cache, BLB_V3(object->position.x, object->position.y, 0.0f), radius,
                                   BLB_OBJECT_CULLING_MARGIN);
}

static bool cached_object3d_visible(BLB_RenderVisibilityCache *cache, BLB_Object3D *object,
                                    const BLB_CameraCache *camera_cache, BLB_Vec3 camera_position) {
  if (!object || !object->visible)
    return false;

  if (object->camera_attached)
    return true;

  if (!cache || (!BLB_OBJECT_CULLING && !BLB_OBJECT_FRUSTUM_CULLING && !BLB_OBJECT_DISTANCE_CULLING))
    return object_visible_to_camera(object, camera_cache, camera_position);

  bool visible = false;
  float distance = 0.0f;

  if (!BLB_RenderVisibilityCache_Get(cache, BLB_VISIBILITY_OBJECT_3D, object, 0, &visible, &distance)) {
    visible = object_visible_to_camera(object, camera_cache, camera_position);
    distance = BLB_LenV3(BLB_SubV3(object->position, camera_position));
    BLB_RenderVisibilityCache_Put(cache, BLB_VISIBILITY_OBJECT_3D, object, 0, visible, distance);
  }

  return visible;
}

static bool cached_object2d_visible(BLB_RenderVisibilityCache *cache, BLB_Object2D *object,
                                    const BLB_CameraCache *camera_cache, BLB_Vec3 camera_position, float viewport_width,
                                    float viewport_height) {
  if (!object || !object->visible)
    return false;

  if (object->camera_attached)
    return object2d_visible_to_camera(object, camera_cache, camera_position, viewport_width, viewport_height);

  if (!cache || (!BLB_OBJECT_CULLING && !BLB_OBJECT_FRUSTUM_CULLING && !BLB_OBJECT_DISTANCE_CULLING))
    return object2d_visible_to_camera(object, camera_cache, camera_position, viewport_width, viewport_height);

  bool visible = false;
  float distance = 0.0f;

  if (!BLB_RenderVisibilityCache_Get(cache, BLB_VISIBILITY_OBJECT_2D, object, 0, &visible, &distance)) {
    visible = object2d_visible_to_camera(object, camera_cache, camera_position, viewport_width, viewport_height);

    if (object->camera_attached) {
      distance = 0.0f;
    } else {
      float dx = object->position.x - camera_position.x;
      float dy = object->position.y - camera_position.y;
      distance = sqrtf(dx * dx + dy * dy);
    }

    BLB_RenderVisibilityCache_Put(cache, BLB_VISIBILITY_OBJECT_2D, object, 0, visible, distance);
  }

  return visible;
}

static bool cluster_visible_to_camera(const BLB_RenderObjects3DCluster *cluster, const BLB_CameraCache *camera_cache,
                                      BLB_Vec3 camera_position) {
  if (!cluster || !cluster->bounds)
    return true;

  if (!BLB_OBJECT_CULLING)
    return true;

  BLB_Vec3 center = BLB_MulV3F(BLB_AddV3(cluster->bounds->min, cluster->bounds->max), 0.5f);

  if (BLB_OBJECT_DISTANCE_CULLING && BLB_OBJECT_MAX_RENDER_DISTANCE > 0.0f) {
    BLB_Vec3 extent = BLB_MulV3F(BLB_SubV3(cluster->bounds->max, cluster->bounds->min), 0.5f);
    float radius = BLB_LenV3(extent);
    BLB_Vec3 delta = BLB_SubV3(center, camera_position);
    float max_distance = BLB_OBJECT_MAX_RENDER_DISTANCE + radius;

    if (BLB_DotV3(delta, delta) > max_distance * max_distance)
      return false;
  }

  if (!BLB_OBJECT_FRUSTUM_CULLING || !camera_cache || !camera_cache->valid)
    return true;

  return view_space_sphere_visible(camera_cache, center,
                                   BLB_LenV3(BLB_MulV3F(BLB_SubV3(cluster->bounds->max, cluster->bounds->min), 0.5f)),
                                   BLB_OBJECT_CULLING_MARGIN);
}

static const Mesh *object_geometry_mesh(const BLB_Object3D *object, Mesh *scratch) {
  if (!object)
    return NULL;

  const BLB_Polygon3D *polygon = active_polygon3d(object);

  if (object->lod_level <= 0 && object->mesh.vertices && object->mesh.indices && object->mesh.vertex_count > 0 &&
      object->mesh.index_count >= 3)
    return &object->mesh;

  if (!scratch || !polygon || !polygon->vertices || !polygon->indices)
    return NULL;

  memset(scratch, 0, sizeof(*scratch));
  scratch->vertices = polygon->vertices;
  scratch->normals = polygon->normals;
  scratch->uvs = polygon->uvs;
  scratch->indices = polygon->indices;
  scratch->vertex_count = polygon->vertex_count;
  scratch->index_count = polygon->index_count;

  return scratch;
}

static bool append_instance_data(BLB_RenderInstanceData *dst, BLB_Object3D *object) {
  if (!dst || !object)
    return false;

  BLB_Mat4 model;
  build_model(object->position, object->rotation, object->scale, &model);
  memcpy(dst->model, model.Elements, sizeof(dst->model));

  dst->color[0] = object->color.x / 255.0f * (object->material ? object->material->base_color[0] : 1.0f);
  dst->color[1] = object->color.y / 255.0f * (object->material ? object->material->base_color[1] : 1.0f);
  dst->color[2] = object->color.z / 255.0f * (object->material ? object->material->base_color[2] : 1.0f);
  dst->color[3] = object->color.w / 255.0f * (object->material ? object->material->base_color[3] : 1.0f);
  dst->entity_id = (uint32_t)object->entity_id;
  dst->padding[0] = 0;
  dst->padding[1] = 0;
  dst->padding[2] = 0;

  return true;
}

typedef struct {
  BLB_Vec3 position;
  BLB_Vec3 rotation;
  BLB_Vec3 scale;
  BLB_RenderInstanceData data;
  bool valid;
} BLB_InstanceTransformCache;

static bool instance_group_reserve(BLB_RenderInstances3DGroup *group, size_t max_batch) {
  if (group->render_batch_capacity < max_batch) {
    void *buffer = realloc(group->render_batch, max_batch * sizeof(BLB_RenderInstanceData));

    if (!buffer)
      return false;
    group->render_batch = buffer;
    group->render_batch_capacity = max_batch;
  }

  if (group->transform_cache_capacity < group->object_count) {
    BLB_InstanceTransformCache *entries = calloc(group->object_count, sizeof(*entries));

    if (!entries)
      return false;
    free(group->transform_cache);
    group->transform_cache = entries;
    group->transform_cache_capacity = group->object_count;
  }

  return true;
}

static void append_cached_instance(BLB_RenderInstanceData *dst, BLB_RenderInstances3DGroup *group, size_t index) {
  BLB_Object3D *object = group->objects[index];
  BLB_InstanceTransformCache *entry = &((BLB_InstanceTransformCache *)group->transform_cache)[index];

  if (!entry->valid || memcmp(&entry->position, &object->position, sizeof(BLB_Vec3)) != 0 ||
      memcmp(&entry->rotation, &object->rotation, sizeof(BLB_Vec3)) != 0 ||
      memcmp(&entry->scale, &object->scale, sizeof(BLB_Vec3)) != 0) {
    append_instance_data(&entry->data, object);
    entry->position = object->position;
    entry->rotation = object->rotation;
    entry->scale = object->scale;
    entry->valid = true;
  }

  *dst = entry->data;
  for (int c = 0; c < 4; ++c)
    dst->color[c] = object->color.Elements[c] / 255.0f * (object->material ? object->material->base_color[c] : 1.0f);

  dst->entity_id = (uint32_t)object->entity_id;
}

static bool build_shadow_matrix(BLB_Scene *scene, BLB_Mat4 *shadow_vp, BLB_Light3D **shadow_light) {
  if (!scene || !shadow_vp)
    return false;

  BLB_Light3D *light = NULL;

  for (int i = 0; i < scene->light3d_count; i++) {
    BLB_Light3D *candidate = scene->lights3d[i];

    if (!candidate || !candidate->enabled || !candidate->object || candidate->type != BLB_LIGHT_DIRECTIONAL)
      continue;

    light = candidate;
    break;
  }

  if (!light)
    return false;

  BLB_Vec3 direction = BLB_GetLightDirection3D(light);
  *shadow_light = light;
  BLB_Vec3 target = BLB_V3(0.0f, 0.0f, -20.0f);

  if (scene->object3d_count > 0) {
    BLB_Vec3 average = BLB_V3(0.0f, 0.0f, 0.0f);
    int count = 0;

    for (int i = 0; i < scene->object3d_count; i++) {
      BLB_Object3D *object = scene->objects3d[i];

      if (!object || !object->visible)
        continue;

      average = BLB_AddV3(average, object->position);
      count++;
    }

    if (count > 0)
      target = BLB_MulV3F(average, 1.0f / (float)count);
  }

  BLB_Vec3 eye = BLB_SubV3(target, BLB_MulV3F(direction, 60.0f));
  BLB_Vec3 up = fabsf(BLB_DotV3(direction, BLB_V3(0.0f, 1.0f, 0.0f))) > 0.98f ? BLB_V3(0.0f, 0.0f, 1.0f)
                                                                              : BLB_V3(0.0f, 1.0f, 0.0f);

  BLB_Mat4 view = BLB_LookAt_RH(eye, target, up);
  BLB_Mat4 projection = BLB_Orthographic_RH_ZO(-35.0f, 35.0f, 35.0f, -35.0f, 0.1f, 140.0f);

  *shadow_vp = BLB_MulM4(projection, view);

  return true;
}

static bool build_point_shadow_matrices(BLB_Scene *scene, BLB_Mat4 shadow_mvp[BLB_RENDER_POINT_SHADOW_FACES],
                                        BLB_Light3D **shadow_light) {
  if (!scene || !shadow_mvp || !shadow_light)
    return false;

  BLB_Light3D *light = NULL;

  for (int i = 0; i < scene->light3d_count; i++) {
    BLB_Light3D *candidate = scene->lights3d[i];

    if (!candidate || !candidate->enabled || !candidate->object)
      continue;

    if (candidate->type != BLB_LIGHT_POINT && candidate->type != BLB_LIGHT_SPOT)
      continue;

    light = candidate;
    break;
  }

  if (!light)
    return false;

  BLB_Vec3 position = light->object->position;
  float far_plane = fmaxf(light->range, 10.0f);
  BLB_Mat4 projection = BLB_Perspective_RH_ZO(BLB_AngleDeg(90.0f), 1.0f, 0.05f, far_plane);

  BLB_Vec3 directions[BLB_RENDER_POINT_SHADOW_FACES] = {BLB_V3(1.0f, 0.0f, 0.0f), BLB_V3(-1.0f, 0.0f, 0.0f),
                                                        BLB_V3(0.0f, 1.0f, 0.0f), BLB_V3(0.0f, -1.0f, 0.0f),
                                                        BLB_V3(0.0f, 0.0f, 1.0f), BLB_V3(0.0f, 0.0f, -1.0f)};

  BLB_Vec3 ups[BLB_RENDER_POINT_SHADOW_FACES] = {BLB_V3(0.0f, -1.0f, 0.0f), BLB_V3(0.0f, -1.0f, 0.0f),
                                                 BLB_V3(0.0f, 0.0f, 1.0f),  BLB_V3(0.0f, 0.0f, -1.0f),
                                                 BLB_V3(0.0f, -1.0f, 0.0f), BLB_V3(0.0f, -1.0f, 0.0f)};

  for (uint32_t i = 0; i < BLB_RENDER_POINT_SHADOW_FACES; i++) {
    BLB_Mat4 view = BLB_LookAt_RH(position, BLB_AddV3(position, directions[i]), ups[i]);
    shadow_mvp[i] = BLB_MulM4(projection, view);
  }

  *shadow_light = light;

  return true;
}

static void renderer_material_set_bounds_3d(BLB_RenderMaterial *material, const BLB_Polygon3D *polygon) {
  if (!material || !polygon || !polygon->vertices || polygon->vertex_count == 0)
    return;

  BLB_Vec3 min_v = polygon->vertices[0];
  BLB_Vec3 max_v = polygon->vertices[0];

  for (size_t i = 1; i < polygon->vertex_count; ++i) {
    BLB_Vec3 v = polygon->vertices[i];
    min_v.x = fminf(min_v.x, v.x);
    min_v.y = fminf(min_v.y, v.y);
    min_v.z = fminf(min_v.z, v.z);
    max_v.x = fmaxf(max_v.x, v.x);
    max_v.y = fmaxf(max_v.y, v.y);
    max_v.z = fmaxf(max_v.z, v.z);
  }
  material->uv_bounds_min[0] = min_v.x;
  material->uv_bounds_min[1] = min_v.y;
  material->uv_bounds_min[2] = min_v.z;
  material->uv_bounds_max[0] = max_v.x;
  material->uv_bounds_max[1] = max_v.y;
  material->uv_bounds_max[2] = max_v.z;
}

static BLB_RenderMaterial renderer_material_from_state(const BLB_RenderMaterialState *state, bool lighting) {
  BLB_RenderMaterial result = {0};

  if (!state)
    return result;

  result.lighting_enabled = lighting && state->lighting_enabled && !state->unlit;
  result.double_sided = state->double_sided;
  result.emission = state->emission;
  result.glow = state->glow;
  result.roundness = state->roughness;
  result.glow_radius = state->glow_radius;
  result.glow_falloff = state->glow_falloff;
  result.metallic = state->metallic;
  result.roughness = state->roughness;
  result.specular = state->specular;
  result.occlusion = state->occlusion;
  result.specular_color[0] = state->specular_color[0];
  result.specular_color[1] = state->specular_color[1];
  result.specular_color[2] = state->specular_color[2];
  result.emission_color[0] = state->emission_color[0];
  result.emission_color[1] = state->emission_color[1];
  result.emission_color[2] = state->emission_color[2];
  result.emission_color[3] = state->emission_color[3];
  result.temperature = state->temperature;
  result.entity_id = state->entity_id;
  result.uv_bounds_min[0] = result.uv_bounds_min[1] = result.uv_bounds_min[2] = -0.5f;
  result.uv_bounds_max[0] = result.uv_bounds_max[1] = result.uv_bounds_max[2] = 0.5f;
  result.normal_scale = state->normal_scale;
  result.ior = state->ior;
  result.transmission = state->transmission;
  result.volume_thickness = state->volume_thickness;
  result.attenuation_color[0] = state->attenuation_color[0];
  result.attenuation_color[1] = state->attenuation_color[1];
  result.attenuation_color[2] = state->attenuation_color[2];
  result.attenuation_distance = state->attenuation_distance;
  result.clearcoat_factor = state->clearcoat_factor;
  result.clearcoat_roughness = state->clearcoat_roughness;
  result.clearcoat_normal_scale = state->clearcoat_normal_scale;
  result.sheen_color[0] = state->sheen_color[0];
  result.sheen_color[1] = state->sheen_color[1];
  result.sheen_color[2] = state->sheen_color[2];
  result.sheen_roughness = state->sheen_roughness;
  result.iridescence_factor = state->iridescence_factor;
  result.iridescence_ior = state->iridescence_ior;
  result.iridescence_thickness_min = state->iridescence_thickness_min;
  result.iridescence_thickness_max = state->iridescence_thickness_max;
  result.anisotropy_strength = state->anisotropy_strength;
  result.anisotropy_rotation = state->anisotropy_rotation;
  result.dispersion = state->dispersion;
  result.alpha_cutoff = state->alpha_cutoff;
  result.alpha_mode = state->alpha_mode;
  result.unlit = state->unlit;
  result.depth_enabled = state->depth_enabled;
  result.depth_write = state->depth_write;
  result.source_material = state->source_material;
  result.render_mode = normalize_render_mode(state->render_mode);

  return result;
}

static void renderer_material_force_overlay(BLB_RenderMaterial *material) {
  if (!material)
    return;

  material->lighting_enabled = false;
  material->unlit = true;
  material->depth_enabled = false;
  material->depth_write = false;
}

static float object3d_lod_distance(const BLB_Object3D *object, BLB_Vec3 camera_position) {
  if (!object)
    return 0.0f;

  if (object->camera_attached)
    return BLB_LenV3(object->position);

  return BLB_LenV3(BLB_SubV3(object->position, camera_position));
}

static int resolve_screen_lod3d(BLB_Object3D *object, const BLB_CameraCache *cache, float distance) {
  int level = BLB_Object3D_ResolveLODLevel(object, distance);

  if (!BLB_OBJECT_LOD || !BLB_OBJECT_LOD_SCREEN_SIZE || !object || object->lod_override || object->camera_attached ||
      !object->polygon ||
      (BLB_OBJECT_LOD_MIN_TRIANGLES > 0 && object->polygon->index_count / 3u <= (size_t)BLB_OBJECT_LOD_MIN_TRIANGLES) ||
      !cache || !cache->valid || !(BLB_OBJECT_LOD_SCREEN_SIZE_START > 0.0f) ||
      !isfinite(BLB_OBJECT_LOD_SCREEN_SIZE_START))
    return level;

  float scale = fmaxf(fabsf(object->scale.x), fmaxf(fabsf(object->scale.y), fabsf(object->scale.z)));
  float radius = object_local_radius(object) * scale;
  float depth = -(cache->view.Elements[0][2] * object->position.x + cache->view.Elements[1][2] * object->position.y +
                  cache->view.Elements[2][2] * object->position.z + cache->view.Elements[3][2]);

  if (depth <= radius || radius <= 0.0f || cache->viewport_height <= 0.0f)
    return level;

  float pixels = radius * fabsf(cache->projection.Elements[1][1]) * cache->viewport_height / (depth - radius);
  int maximum = BLB_OBJECT_LOD_LEVELS > 31 ? 31 : BLB_OBJECT_LOD_LEVELS;
  int screen_level = 0;
  float threshold = BLB_OBJECT_LOD_SCREEN_SIZE_START;

  while (screen_level < maximum && pixels < threshold) {
    ++screen_level;
    threshold *= 0.5f;
  }

  return screen_level > level ? screen_level : level;
}

static const BLB_Polygon3D *resolve_object_polygon3d(BLB_Object3D *object, BLB_Vec3 camera_position,
                                                     const BLB_CameraCache *cache) {
  if (!object || !object->polygon)
    return NULL;

  if (object->type == BLB_OBJECT_CUSTOM_MODEL && (object->submesh_count > 1 || object->animation3d))
    return object->polygon;

  float distance = object3d_lod_distance(object, camera_position);
  int level = resolve_screen_lod3d(object, cache, distance);

  if (level > 0)
    BLB_Object3D_EnsureLODLevel(object, level);

  return BLB_Object3D_GetLODPolygonAtLevel(object, level);
}

// drawing functions
static bool object_has_dynamic_physics(const BLB_Object3D *object) {
  if (!object || !object->rigid_body)
    return false;

  BLB_Physics3DBodyType type = BLB_Physics3D_BodyGetType(object);

  return type == BLB_PHYSICS3D_DYNAMIC || type == BLB_PHYSICS3D_KINEMATIC;
}

static bool object_should_use_instance_path(const BLB_Object3D *object) {
  if (!object)
    return false;

  if (object_has_dynamic_physics(object))
    return true;

  if (object->optimization_transform_dirty)
    return true;

  return BLB_OBJECT_SMART_OPTIMIZATION && object->optimization_dynamic;
}

static void update_object_motion_state(BLB_Object3D *object, BLB_Scene *scene) {
  if (!object)
    return;

  bool physics_dynamic = object_has_dynamic_physics(object);

  if (!object->optimization_motion_initialized) {
    object->optimization_last_position = object->position;
    object->optimization_last_rotation = object->rotation;
    object->optimization_last_scale = object->scale;
    object->optimization_motion_initialized = true;
    object->optimization_dynamic = physics_dynamic || object->optimization_transform_dirty;
    object->optimization_stable_frames = 0;
  }

  bool changed = memcmp(&object->position, &object->optimization_last_position, sizeof(BLB_Vec3)) != 0 ||
                 memcmp(&object->rotation, &object->optimization_last_rotation, sizeof(BLB_Vec3)) != 0 ||
                 memcmp(&object->scale, &object->optimization_last_scale, sizeof(BLB_Vec3)) != 0;

  if (changed && object->optimization_instance_group) {
    BLB_RenderInstances3DGroup *group = object->optimization_instance_group;
    group->cull_blocks_dirty = true;
    group->bounds_valid = false;
  }

  bool moved = false;

  if (changed) {
    moved = BLB_LenSqrV3(BLB_SubV3(object->position, object->optimization_last_position)) >
                BLB_OBJECT_OPTIMIZATION_MOTION_POSITION_EPSILON * BLB_OBJECT_OPTIMIZATION_MOTION_POSITION_EPSILON ||
            BLB_LenSqrV3(BLB_SubV3(object->rotation, object->optimization_last_rotation)) >
                BLB_OBJECT_OPTIMIZATION_MOTION_ROTATION_EPSILON * BLB_OBJECT_OPTIMIZATION_MOTION_ROTATION_EPSILON ||
            BLB_LenSqrV3(BLB_SubV3(object->scale, object->optimization_last_scale)) >
                BLB_OBJECT_OPTIMIZATION_MOTION_POSITION_EPSILON * BLB_OBJECT_OPTIMIZATION_MOTION_POSITION_EPSILON;

    object->optimization_last_position = object->position;
    object->optimization_last_rotation = object->rotation;
    object->optimization_last_scale = object->scale;
  }

  if (!BLB_OBJECT_SMART_OPTIMIZATION)
    return;

  if (physics_dynamic || moved || object->optimization_transform_dirty) {
    if (moved && !object->optimization_transform_dirty) {
      ++object->transform_revision;

      if (!object->transform_revision)
        object->transform_revision = 1;
    }

    if (object->optimization_cluster)
      object->optimization_cluster->dirty = true;

    if (object->optimization_instance_group)
      ((BLB_RenderInstances3DGroup *)object->optimization_instance_group)->bounds_valid = false;

    if (object->optimization_instance_group)
      ((BLB_RenderInstances3DGroup *)object->optimization_instance_group)->cull_blocks_dirty = true;

    object->optimization_dynamic = true;
    object->optimization_stable_frames = 0;
    object->optimization_static_recovered = false;
    object->optimization_transform_dirty = false;

    if (scene && BLB_OBJECT_AGGREGATION && BLB_OBJECT_AGGREGATION_REBUILD_ON_TRANSFORM_CHANGE)
      scene->optimization_rebuild_pending = true;

    return;
  }

  if (!object->optimization_dynamic)
    return;

  if (object->optimization_stable_frames < UINT32_MAX)
    object->optimization_stable_frames++;

  uint32_t recover_frames =
      BLB_OBJECT_OPTIMIZATION_STATIC_RECOVER_FRAMES > 0 ? (uint32_t)BLB_OBJECT_OPTIMIZATION_STATIC_RECOVER_FRAMES : 1u;

  if (object->optimization_stable_frames >= recover_frames) {
    object->optimization_dynamic = false;
    object->optimization_static_recovered = true;
    object->optimization_stable_frames = 0;

    if (scene && BLB_OBJECT_AGGREGATION && BLB_OBJECT_AGGREGATION_REBUILD_ON_TRANSFORM_CHANGE)
      scene->optimization_rebuild_pending = true;
  }
}

static void draw_object3d_pass(BLB_Object3D *object, const BLB_Polygon3D *polygon, BLB_Renderer *renderer,
                               const BLB_CameraCache *camera_cache, BLB_Vec3 camera_position,
                               const BLB_RenderMaterialState *state, float scale_mul, float glow_mul) {
  if (!object || !polygon || !renderer || !camera_cache || !camera_cache->valid || !state)
    return;

  (void)camera_position;
  BLB_Mat4 model = blb_render_object3d_model_matrix(object, camera_cache, scale_mul);
  BLB_Mat4 mvp = BLB_MulM4(camera_cache->projection, BLB_MulM4(camera_cache->view, model));
  float model_rows[12];
  extract_matrix_rows(&model, model_rows);

  BLB_RenderMaterialState pass = *state;
  bool glow_pass = glow_mul < 0.9999f;

  pass.emission *= glow_mul;
  pass.glow *= glow_mul;
  pass.base_color[3] *= glow_mul;

  if (glow_pass) {
    pass.lighting_enabled = false;
    pass.emission = glow_mul;
    pass.glow = 0.0f;
    pass.base_color[0] = pass.emission_color[0];
    pass.base_color[1] = pass.emission_color[1];
    pass.base_color[2] = pass.emission_color[2];
    pass.render_mode = BLB_RENDER_ADDITIVE;
  } else if (pass.alpha_mode == BLB_ALPHA_BLEND && pass.render_mode == BLB_RENDER_OPAQUE) {
    pass.render_mode = BLB_RENDER_TRANSPARENT;
  }

  BLB_RenderMaterial material = renderer_material_from_state(&pass, !glow_pass);
  renderer_material_set_bounds_3d(&material, polygon);
  material.base_texture_override = object->texture;

  if (object->camera_attached)
    renderer_material_force_overlay(&material);

  BLB_RendererDrawPolygon3D(renderer, polygon, &mvp.Elements[0][0], model_rows, pass.base_color[0], pass.base_color[1],
                            pass.base_color[2], pass.base_color[3], &material, object->texture);
}

static BLB_Polygon3D *skybox_plane_polygon(void) {
  static BLB_Vec3 vertices[3] = {
      {.x = -1.0f, .y = -1.0f, .z = 0.0f},
      {.x = 3.0f, .y = -1.0f, .z = 0.0f},
      {.x = -1.0f, .y = 3.0f, .z = 0.0f},
  };

  static BLB_Vec3 normals[3] = {
      {.x = 0.0f, .y = 0.0f, .z = 1.0f},
      {.x = 0.0f, .y = 0.0f, .z = 1.0f},
      {.x = 0.0f, .y = 0.0f, .z = 1.0f},
  };

  static BLB_Vec2 uvs[3] = {
      {.x = 0.0f, .y = 0.0f},
      {.x = 0.0f, .y = 0.0f},
      {.x = 0.0f, .y = 0.0f},
  };

  static unsigned int indices[3] = {0, 1, 2};

  static BLB_Polygon3D polygon = {
      .vertices = vertices,
      .base_vertices = vertices,
      .normals = normals,
      .uvs = uvs,
      .vertex_count = 3,
      .indices = indices,
      .index_count = 3,
      .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f},
      .geometry_hash = 0x534B595246554C4Cull,
  };

  return &polygon;
}

static void draw_skybox(BLB_Skybox *skybox, BLB_Renderer *renderer, const BLB_Camera *camera,
                        const BLB_CameraCache *camera_cache) {
  if (!skybox || !skybox->object || !renderer || !camera || !camera_cache || !camera_cache->valid)
    return;

  BLB_Object3D *object = skybox->object;

  if (!object->material || !object->texture)
    return;

  float pitch = camera->rotation.x * BLB_PI / 180.0f;
  float yaw = camera->rotation.y * BLB_PI / 180.0f;
  BLB_Vec3 right = BLB_V3(cosf(yaw), 0.0f, sinf(yaw));
  BLB_Vec3 up = BLB_V3(sinf(yaw) * sinf(pitch), cosf(pitch), -cosf(yaw) * sinf(pitch));
  BLB_Vec3 forward = BLB_V3(cosf(pitch) * sinf(yaw), -sinf(pitch), -cosf(pitch) * cosf(yaw));

  float x_scale = fabsf(camera_cache->projection.Elements[0][0]) > 0.000001f
                      ? 1.0f / fabsf(camera_cache->projection.Elements[0][0])
                      : 1.0f;

  /* The Vulkan projection flips its Y coefficient. Use its magnitude here;
   * screen-to-ray Y orientation is handled explicitly in the skybox shader. */
  float projection_y_scale = fabsf(camera_cache->projection.Elements[1][1]);
  float y_scale = projection_y_scale > 0.000001f ? 1.0f / projection_y_scale : 1.0f;

  float model_rows[12] = {right.x * x_scale, right.y * x_scale, right.z * x_scale, 0.0f,
                          up.x * y_scale,    up.y * y_scale,    up.z * y_scale,    0.0f,
                          forward.x,         forward.y,         forward.z,         0.0f};

  BLB_Mat4 identity = BLB_M4D(1.0f);

  BLB_RenderMaterialState state = material_state_from_3d(object);
  state.lighting_enabled = false;
  state.unlit = true;
  state.depth_enabled = false;
  state.depth_write = false;
  state.double_sided = true;
  state.alpha_mode = BLB_ALPHA_OPAQUE;
  state.render_mode = BLB_RENDER_OPAQUE;

  BLB_RenderMaterial material = renderer_material_from_state(&state, false);
  material.base_texture_override = object->texture;
  material.skybox = true;
  renderer_material_set_bounds_3d(&material, object->polygon);

  BLB_RendererDrawPolygon3D(renderer, skybox_plane_polygon(), &identity.Elements[0][0], model_rows, 1.0f, 1.0f, 1.0f,
                            1.0f, &material, object->texture);
}

static void draw_object3d_with_state(BLB_Object3D *object, const BLB_Polygon3D *polygon, BLB_Renderer *renderer,
                                     const BLB_CameraCache *camera_cache, BLB_Vec3 camera_position,
                                     const BLB_RenderMaterialState *state) {
  if (!object || !polygon || !renderer || !camera_cache || !camera_cache->valid || !state)
    return;

  if (state->glow > 0.0f && state->glow_radius > 0.0f) {
    const int steps = BLB_GLOW_STEPS_3D > 0 ? BLB_GLOW_STEPS_3D : 1;
    const float radius = state->glow_radius;
    const float falloff = fmaxf(state->glow_falloff, 0.2f);

    for (int i = 1; i <= steps; i++) {
      float t = (float)i / (float)steps;
      float envelope = powf(fmaxf(0.0f, 1.0f - t), falloff);
      float smooth_envelope = envelope * (0.92f + 0.08f * (1.0f - t));
      float scale_mul = 1.0f + radius * 0.019f * t;
      float glow_mul = smooth_envelope * state->glow * 0.22f;

      draw_object3d_pass(object, polygon, renderer, camera_cache, camera_position, state, scale_mul, glow_mul);
    }
  }

  draw_object3d_pass(object, polygon, renderer, camera_cache, camera_position, state, 1.0f, 1.0f);
}

static void draw_object3d(BLB_Object3D *object, BLB_Renderer *renderer, const BLB_CameraCache *camera_cache,
                          BLB_Vec3 camera_position) {
  if (!object || !renderer || !camera_cache || !camera_cache->valid || !object->visible)
    return;

  if (object->type == BLB_OBJECT_CUSTOM_MODEL && object->submeshes &&
      (object->submesh_count > 1 || object->animation3d)) {
    for (size_t i = 0; i < object->submesh_count; ++i) {
      BLB_ObjectSubmesh *submesh = &object->submeshes[i];

      if (!submesh->polygon)
        continue;

      BLB_Object3D submesh_object = *object;
      submesh_object.polygon = submesh->polygon;
      submesh_object.material = (object->material && object->material != object->submeshes[0].material)
                                    ? object->material
                                    : submesh->material;

      BLB_RenderMaterialState state = material_state_from_3d(&submesh_object);
      draw_object3d_with_state(&submesh_object, submesh->polygon, renderer, camera_cache, camera_position, &state);
    }

    return;
  }

  const BLB_Polygon3D *polygon = resolve_object_polygon3d(object, camera_position, camera_cache);

  if (!polygon)
    return;

  BLB_RenderMaterialState state = material_state_from_3d(object);
  draw_object3d_with_state(object, polygon, renderer, camera_cache, camera_position, &state);
}

static bool append_instance_data2d(BLB_RenderInstanceData *dst, const BLB_Object2D *object,
                                   const BLB_CameraCache *camera_cache) {
  if (!dst || !object)
    return false;

  const float angle = BLB_AngleDeg(object->rotation);
  const float c = object->rotation == 0.0f ? 1.0f : cosf(angle);
  const float s = object->rotation == 0.0f ? 0.0f : sinf(angle);
  BLB_Mat4 model = BLB_M4D(1.0f);
  model.Elements[0][0] = c * object->scale.x;
  model.Elements[0][1] = s * object->scale.x;
  model.Elements[1][0] = -s * object->scale.y;
  model.Elements[1][1] = c * object->scale.y;
  model.Elements[3][0] = object->position.x;
  model.Elements[3][1] = object->position.y;
  model.Elements[3][2] = 0.0f;
  (void)camera_cache;

  memset(dst, 0, sizeof(*dst));
  memcpy(dst->model, model.Elements, sizeof(dst->model));
  dst->color[0] = object->color.x / 255.0f * (object->material ? object->material->base_color[0] : 1.0f);
  dst->color[1] = object->color.y / 255.0f * (object->material ? object->material->base_color[1] : 1.0f);
  dst->color[2] = object->color.z / 255.0f * (object->material ? object->material->base_color[2] : 1.0f);
  dst->color[3] = object->color.w / 255.0f * (object->material ? object->material->base_color[3] : 1.0f);
  dst->entity_id = (uint32_t)object->entity_id;

  return true;
}

static bool instanced_group2d_matches_attached(const BLB_RenderInstances2DGroup *group, bool camera_attached) {
  return group && group->enabled && group->camera_attached == camera_attached && group->object_count >= 2u;
}

static void draw_instanced_group2d(BLB_RenderInstances2DGroup *group, BLB_Renderer *renderer,
                                   const BLB_CameraCache *camera_cache, BLB_Scene *scene) {
  if (!group || !group->enabled || group->object_count == 0 || !renderer || !scene)
    return;

  BLB_Object2D *source = group->objects[0];
  const BLB_Polygon2D *polygon = active_polygon2d(source);

  if (!source || !polygon || !source->material)
    return;

  uint32_t viewport_width_u = 0;
  uint32_t viewport_height_u = 0;
  BLB_RendererGetViewport(renderer, &viewport_width_u, &viewport_height_u);
  float width = (float)viewport_width_u;
  float height = (float)viewport_height_u;

  size_t max_batch = BLB_OBJECT_INSTANCING_MAX_INSTANCES > 0 ? (size_t)BLB_OBJECT_INSTANCING_MAX_INSTANCES : 1u;

  if (max_batch > group->object_count)
    max_batch = group->object_count;

  if (group->batch_capacity < max_batch)
    max_batch = group->batch_capacity;

  if (!group->batch || max_batch == 0)
    return;

  BLB_RenderInstanceData *batch = group->batch;

  BLB_RenderMaterialState state = material_state_from_2d(source);
  BLB_RenderMaterial material = renderer_material_from_state(&state, true);
  BLB_Texture *texture = BLB_Object2D_GetTexture(source);

  material.base_texture_override = texture;

  if (state.alpha_mode == BLB_ALPHA_BLEND && material.render_mode == BLB_RENDER_OPAQUE)
    material.render_mode = BLB_RENDER_TRANSPARENT;

  if (source->camera_attached)
    renderer_material_force_overlay(&material);

  size_t count = 0;

  for (size_t i = 0; i < group->object_count; ++i) {
    BLB_Object2D *object = group->objects[i];

    if (!object || !object->visible)
      continue;

    if (BLB_OBJECT_CULLING || BLB_OBJECT_FRUSTUM_CULLING || BLB_OBJECT_DISTANCE_CULLING) {
      if (!cached_object2d_visible(scene->visibility_cache, object, camera_cache,
                                   scene->camera ? scene->camera->position : BLB_V3(0, 0, 0), width, height))
        continue;
    }

    BLB_Texture *next_texture = BLB_Object2D_GetTexture(object);

    if (count && (count == max_batch || next_texture != texture)) {
      BLB_RendererDrawInstancedPolygon2D(renderer, polygon, width, height, &material, texture, batch, count,
                                         group->camera_attached);
      count = 0;
    }

    texture = next_texture;
    material.base_texture_override = texture;

    if (append_instance_data2d(&batch[count], object, camera_cache))
      ++count;
  }

  if (count > 0)
    BLB_RendererDrawInstancedPolygon2D(renderer, polygon, width, height, &material, texture, batch, count,
                                       group->camera_attached);
}

static BLB_Vec3 safe_normalize_local(BLB_Vec3 value, BLB_Vec3 fallback) {
  float length = BLB_LenV3(value);

  return length > 0.000001f ? BLB_MulV3F(value, 1.0f / length) : fallback;
}

static bool particle3d_visible_to_camera(const BLB_Particle3D *particle, const BLB_CameraCache *camera_cache) {
  if (!particle || particle->camera_attached || !camera_cache || !camera_cache->valid)
    return true;

  float sx = fabsf(particle->scale.x);
  float sy = fabsf(particle->scale.y);
  float sz = fabsf(particle->scale.z);
  float radius = 0.5f * sqrtf(fmaxf(sx * sx + sy * sy, sz * sz));

  return view_space_sphere_visible(camera_cache, particle->position, radius, BLB_OBJECT_CULLING_MARGIN);
}

static void draw_particle2d(BLB_Particle2D *particle, BLB_Renderer *renderer, float viewport_width,
                            float viewport_height) {
  if (!particle || !renderer || !particle->visible || !particle->polygon)
    return;

  BLB_RenderMaterialState state = {0};

  if (particle->material) {
    BLB_Object2D proxy = {0};
    proxy.color = BLB_V4(255, 255, 255, 255);
    proxy.material = particle->material;
    proxy.entity_id = particle->id.id;
    state = material_state_from_2d(&proxy);
  }

  BLB_RenderMaterial material = renderer_material_from_state(&state, false);
  material.particle = true;
  material.base_texture_override = particle->texture;

  if (particle->camera_attached)
    renderer_material_force_overlay(&material);

  BLB_RenderInstanceData instance = {0};
  BLB_Mat4 rotation = BLB_Rotate_RH(BLB_AngleDeg(particle->rotation), BLB_V3(0, 0, 1));
  BLB_Mat4 model = BLB_MulM4(BLB_Translate(BLB_V3(particle->position.x, particle->position.y, 0.0f)),
                             BLB_MulM4(rotation, BLB_Scale(BLB_V3(particle->scale.x, particle->scale.y, 1.0f))));
  memcpy(instance.model, model.Elements, sizeof(instance.model));

  instance.color[0] = particle->color.x / 255.0f * state.base_color[0];
  instance.color[1] = particle->color.y / 255.0f * state.base_color[1];
  instance.color[2] = particle->color.z / 255.0f * state.base_color[2];
  instance.color[3] = particle->color.w / 255.0f * state.base_color[3];

  instance.entity_id = particle->id.id;

  BLB_RendererDrawInstancedPolygon2D(renderer, particle->polygon, viewport_width, viewport_height, &material,
                                     particle->texture, &instance, 1, particle->camera_attached);
}

static BLB_Mat4 particle_billboard_model_3d(BLB_Vec3 position, BLB_Vec3 scale, float roll, BLB_Vec3 camera_position) {
  BLB_Vec3 normal = safe_normalize_local(BLB_SubV3(camera_position, position), BLB_V3(0.0f, 0.0f, 1.0f));

  BLB_Vec3 world_up = fabsf(normal.y) > 0.985f ? BLB_V3(1.0f, 0.0f, 0.0f) : BLB_V3(0.0f, 1.0f, 0.0f);

  BLB_Vec3 right = safe_normalize_local(BLB_Cross(world_up, normal), BLB_V3(1.0f, 0.0f, 0.0f));
  BLB_Vec3 up = safe_normalize_local(BLB_Cross(normal, right), BLB_V3(0.0f, 1.0f, 0.0f));

  float c = cosf(BLB_ToRad(roll));
  float s = sinf(BLB_ToRad(roll));

  BLB_Vec3 r = BLB_AddV3(BLB_MulV3F(right, c), BLB_MulV3F(up, s));
  BLB_Vec3 u = BLB_AddV3(BLB_MulV3F(up, c), BLB_MulV3F(right, -s));

  BLB_Mat4 model = BLB_M4D(1.0f);

  model.Elements[0][0] = r.x * scale.x;
  model.Elements[0][1] = r.y * scale.x;
  model.Elements[0][2] = r.z * scale.x;
  model.Elements[1][0] = u.x * scale.y;
  model.Elements[1][1] = u.y * scale.y;
  model.Elements[1][2] = u.z * scale.y;
  model.Elements[3][0] = position.x;
  model.Elements[3][1] = position.y;
  model.Elements[3][2] = position.z;

  return model;
}

static void draw_particle3d(BLB_Particle3D *particle, BLB_Renderer *renderer, const BLB_CameraCache *camera_cache,
                            BLB_Vec3 camera_position) {
  if (!particle || !renderer || !camera_cache || !camera_cache->valid || !particle->visible || !particle->mesh)
    return;

  BLB_RenderMaterialState state = {0};

  if (particle->material) {
    BLB_Object3D proxy = {0};
    proxy.color = BLB_V4(255, 255, 255, 255);
    proxy.material = particle->material;
    proxy.entity_id = particle->id.id;
    state = material_state_from_3d(&proxy);
  }

  BLB_RenderMaterial material = renderer_material_from_state(&state, !state.unlit);
  material.particle = true;
  material.base_texture_override = particle->texture;

  BLB_Mat4 model;

  if (particle->camera_attached) {
    build_model(particle->position, particle->rotation, particle->scale, &model);
    model = BLB_MulM4(BLB_CameraPixelToWorld(camera_cache), model);
    renderer_material_force_overlay(&material);
  } else {
    model = particle_billboard_model_3d(particle->position, particle->scale, particle->rotation.z, camera_position);
  }

  BLB_RenderInstanceData instance = {0};
  memcpy(instance.model, model.Elements, sizeof(instance.model));
  instance.color[0] = particle->color.x / 255.0f * state.base_color[0];
  instance.color[1] = particle->color.y / 255.0f * state.base_color[1];
  instance.color[2] = particle->color.z / 255.0f * state.base_color[2];
  instance.color[3] = particle->color.w / 255.0f * state.base_color[3];
  instance.entity_id = particle->id.id;

  BLB_Mat4 identity = BLB_M4D(1.0f);
  BLB_Mat4 mvp = BLB_MulM4(camera_cache->projection, BLB_MulM4(camera_cache->view, identity));
  BLB_RendererDrawInstancedMesh(renderer, particle->mesh, &mvp.Elements[0][0], &material, particle->texture, &instance,
                                1, camera_position);
}

static void draw_particle_system3d(BLB_ParticleSystem3D *system, BLB_Renderer *renderer,
                                   const BLB_CameraCache *camera_cache, BLB_Vec3 camera_position) {
  if (!system || !renderer || !camera_cache || !camera_cache->valid || !system->visible || system->count == 0)
    return;

  if (!BLB_PARTICLE_INSTANCING) {
    for (size_t i = 0; i < system->count; ++i) {
      BLB_Particle3D *particle = &system->particles[i];

      if (particle && particle->visible &&
          (!BLB_PARTICLE_CULLING || particle3d_visible_to_camera(particle, camera_cache)))
        draw_particle3d(particle, renderer, camera_cache, camera_position);
    }

    return;
  }

  BLB_Particle3D *first = NULL;
  size_t visible_count = 0;

  for (size_t i = 0; i < system->count; ++i) {
    BLB_Particle3D *p = &system->particles[i];

    if (!p->visible || !p->mesh)
      continue;

    if (BLB_PARTICLE_CULLING && !particle3d_visible_to_camera(p, camera_cache))
      continue;

    if (!first)
      first = p;

    ++visible_count;
  }

  if (!first || visible_count == 0)
    return;

  BLB_RenderMaterialState state = {0};

  if (system->material) {
    BLB_Object3D proxy = {0};
    proxy.color = BLB_V4(255, 255, 255, 255);
    proxy.material = system->material;
    proxy.entity_id = system->id.id;
    state = material_state_from_3d(&proxy);
  }

  state.entity_id = (float)system->id.id;
  BLB_RenderMaterial material = renderer_material_from_state(&state, !state.unlit);
  material.base_texture_override = system->texture ? system->texture : first->texture;
  material.particle = true;

  if (first->camera_attached)
    renderer_material_force_overlay(&material);

  size_t max_batch = BLB_PARTICLE_INSTANCING_MAX_INSTANCES > 0 ? (size_t)BLB_PARTICLE_INSTANCING_MAX_INSTANCES : 1u;

  if (max_batch > visible_count)
    max_batch = visible_count;

  BLB_RenderInstanceData *batch = malloc(max_batch * sizeof(*batch));

  if (!batch)
    return;

  BLB_Mat4 identity = BLB_M4D(1.0f);
  BLB_Mat4 mvp = BLB_MulM4(camera_cache->projection, BLB_MulM4(camera_cache->view, identity));
  size_t count = 0;

  for (size_t i = 0; i < system->count; ++i) {
    BLB_Particle3D *p = &system->particles[i];

    if (!p->visible || !p->mesh)
      continue;

    if (BLB_PARTICLE_CULLING && !particle3d_visible_to_camera(p, camera_cache))
      continue;

    if (p->mesh != first->mesh || p->material != first->material || p->texture != first->texture ||
        p->camera_attached != first->camera_attached) {
      draw_particle3d(p, renderer, camera_cache, camera_position);
      continue;
    }

    BLB_Mat4 model;

    if (p->camera_attached) {
      build_model(p->position, p->rotation, p->scale, &model);
      model = BLB_MulM4(BLB_CameraPixelToWorld(camera_cache), model);
    } else {
      model = particle_billboard_model_3d(p->position, p->scale, p->rotation.z, camera_position);
    }

    memcpy(batch[count].model, model.Elements, sizeof(batch[count].model));
    batch[count].color[0] = p->color.x / 255.0f * state.base_color[0];
    batch[count].color[1] = p->color.y / 255.0f * state.base_color[1];
    batch[count].color[2] = p->color.z / 255.0f * state.base_color[2];
    batch[count].color[3] = p->color.w / 255.0f * state.base_color[3];
    batch[count].entity_id = p->id.id;
    batch[count].padding[0] = batch[count].padding[1] = batch[count].padding[2] = 0;
    ++count;

    if (count == max_batch) {
      BLB_RendererDrawInstancedMesh(renderer, first->mesh, &mvp.Elements[0][0], &material,
                                    material.base_texture_override, batch, count, camera_position);
      count = 0;
    }
  }

  if (count > 0)
    BLB_RendererDrawInstancedMesh(renderer, first->mesh, &mvp.Elements[0][0], &material, material.base_texture_override,
                                  batch, count, camera_position);

  free(batch);
}

static bool particle2d_visible_to_camera(const BLB_Particle2D *particle, const BLB_CameraCache *camera_cache,
                                         float viewport_width, float viewport_height) {
  if (!particle || !particle->visible)
    return false;

  float radius = 0.5f * fmaxf(fabsf(particle->scale.x), fabsf(particle->scale.y)) * 1.41421356237f;
  radius *= 1.0f + fmaxf(BLB_OBJECT_CULLING_MARGIN, 0.0f);

  if (particle->camera_attached) {
    return particle->position.x + radius >= 0.0f && particle->position.x - radius <= viewport_width &&
           particle->position.y + radius >= 0.0f && particle->position.y - radius <= viewport_height;
  }

  if (!camera_cache || !camera_cache->valid)
    return true;

  return view_space_sphere_visible(camera_cache, BLB_V3(particle->position.x, particle->position.y, 0.0f), radius,
                                   0.0f);
}

static void draw_particle_system2d(BLB_ParticleSystem2D *system, BLB_Renderer *renderer,
                                   const BLB_CameraCache *camera_cache, float viewport_width, float viewport_height) {
  if (!system || !renderer || !system->visible || system->count == 0)
    return;

  if (!BLB_PARTICLE_INSTANCING) {
    for (size_t i = 0; i < system->count; ++i) {
      BLB_Particle2D *particle = &system->particles[i];

      if (particle && particle->visible &&
          (!BLB_PARTICLE_CULLING ||
           particle2d_visible_to_camera(particle, camera_cache, viewport_width, viewport_height)))
        draw_particle2d(particle, renderer, viewport_width, viewport_height);
    }

    return;
  }

  BLB_Particle2D *first = NULL;
  size_t visible_count = 0;

  for (size_t i = 0; i < system->count; ++i) {
    BLB_Particle2D *p = &system->particles[i];

    if (!p->visible || !p->polygon)
      continue;

    if (BLB_PARTICLE_CULLING && !particle2d_visible_to_camera(p, camera_cache, viewport_width, viewport_height))
      continue;

    if (!first)
      first = p;
    ++visible_count;
  }

  if (!first || visible_count == 0)
    return;

  BLB_RenderMaterialState state = {0};
  state.base_color[0] = state.base_color[1] = state.base_color[2] = state.base_color[3] = 1.0f;
  state.roughness = 0.65f;
  state.specular = 1.0f;
  state.occlusion = 1.0f;
  state.temperature = 6500.0f;
  state.specular_color[0] = state.specular_color[1] = state.specular_color[2] = 1.0f;
  state.emission_color[0] = state.emission_color[1] = state.emission_color[2] = state.emission_color[3] = 1.0f;
  state.attenuation_color[0] = state.attenuation_color[1] = state.attenuation_color[2] = 1.0f;
  state.render_mode = normalize_render_mode((BLB_RenderMode)system->render_mode);
  state.alpha_mode = BLB_ALPHA_BLEND;
  state.lighting_enabled = false;
  state.depth_enabled = false;
  state.depth_write = false;
  state.unlit = true;

  if (system->material) {
    const BLB_Material *m = system->material;
    state.base_color[0] = m->base_color[0];
    state.base_color[1] = m->base_color[1];
    state.base_color[2] = m->base_color[2];
    state.base_color[3] = m->base_color[3];
    state.emission = m->emission_strength;
    state.glow = m->glow_strength;
    state.glow_radius = m->glow_radius;
    state.glow_falloff = m->glow_falloff;
    state.alpha_mode = m->alpha_mode;
    state.alpha_cutoff = m->alpha_cutoff;
    state.render_mode = m->render_mode;
    state.source_material = m;
  }

  state.entity_id = (float)system->id.id;
  BLB_RenderMaterial material = renderer_material_from_state(&state, false);
  material.base_texture_override = system->texture ? system->texture : first->texture;
  material.particle = true;

  if (first->camera_attached)
    renderer_material_force_overlay(&material);

  size_t max_batch = BLB_PARTICLE_INSTANCING_MAX_INSTANCES > 0 ? (size_t)BLB_PARTICLE_INSTANCING_MAX_INSTANCES : 1u;

  if (max_batch > visible_count)
    max_batch = visible_count;

  BLB_RenderInstanceData *batch = malloc(max_batch * sizeof(*batch));

  if (!batch)
    return;

  size_t count = 0;

  for (size_t i = 0; i < system->count; ++i) {
    BLB_Particle2D *p = &system->particles[i];

    if (!p->visible || !p->polygon)
      continue;

    if (BLB_PARTICLE_CULLING && !particle2d_visible_to_camera(p, camera_cache, viewport_width, viewport_height))
      continue;

    if (p->polygon != first->polygon || p->material != first->material || p->texture != first->texture ||
        p->camera_attached != first->camera_attached) {
      draw_particle2d(p, renderer, viewport_width, viewport_height);
      continue;
    }

    BLB_Mat4 rotation = BLB_Rotate_RH(BLB_AngleDeg(p->rotation), BLB_V3(0, 0, 1));
    BLB_Mat4 model = BLB_MulM4(BLB_Translate(BLB_V3(p->position.x, p->position.y, 0.0f)),
                               BLB_MulM4(rotation, BLB_Scale(BLB_V3(p->scale.x, p->scale.y, 1.0f))));
    memcpy(batch[count].model, model.Elements, sizeof(batch[count].model));
    batch[count].color[0] = p->color.x / 255.0f * state.base_color[0];
    batch[count].color[1] = p->color.y / 255.0f * state.base_color[1];
    batch[count].color[2] = p->color.z / 255.0f * state.base_color[2];
    batch[count].color[3] = p->color.w / 255.0f * state.base_color[3];
    batch[count].entity_id = (uint32_t)p->id.id;
    batch[count].padding[0] = batch[count].padding[1] = batch[count].padding[2] = 0;
    ++count;

    if (count == max_batch) {
      BLB_RendererDrawInstancedPolygon2D(renderer, first->polygon, viewport_width, viewport_height, &material,
                                         material.base_texture_override, batch, count, first->camera_attached);
      count = 0;
    }
  }

  if (count > 0)
    BLB_RendererDrawInstancedPolygon2D(renderer, first->polygon, viewport_width, viewport_height, &material,
                                       material.base_texture_override, batch, count, first->camera_attached);
  free(batch);
}

static void draw_object2d_pass(BLB_Object2D *object, BLB_Renderer *renderer, const BLB_CameraCache *camera_cache,
                               const BLB_RenderMaterialState *state, float scale_mul, float glow_mul) {
  const BLB_Polygon2D *polygon = active_polygon2d(object);

  if (!object || !renderer || !polygon || !state)
    return;

  if (!object->camera_attached && (!camera_cache || !camera_cache->valid))
    return;

  BLB_RenderMaterialState pass = *state;
  const bool glow_pass = glow_mul < 0.9999f;

  pass.emission *= glow_mul;
  pass.glow *= glow_mul;
  pass.base_color[3] *= glow_mul;

  if (glow_pass) {
    pass.lighting_enabled = false;
    pass.emission = glow_mul;
    pass.glow = 0.0f;
    pass.base_color[0] = pass.emission_color[0];
    pass.base_color[1] = pass.emission_color[1];
    pass.base_color[2] = pass.emission_color[2];
    pass.render_mode = BLB_RENDER_ADDITIVE;
  } else if (pass.alpha_mode == BLB_ALPHA_BLEND && pass.render_mode == BLB_RENDER_OPAQUE) {
    pass.render_mode = BLB_RENDER_TRANSPARENT;
  }

  BLB_RenderMaterial material = renderer_material_from_state(&pass, !glow_pass);

  if (object->camera_attached) {
    material.depth_enabled = false;
    material.depth_write = false;
  }

  if (!polygon->vertices || !polygon->indices || polygon->vertex_count == 0)
    return;

  BLB_RenderInstanceData instance;

  if (!append_instance_data2d(&instance, object, camera_cache))
    return;

  for (size_t column = 0; column < 2; ++column)
    for (size_t row = 0; row < 4; ++row)
      instance.model[column * 4 + row] *= scale_mul;

  memcpy(instance.color, pass.base_color, sizeof(instance.color));
  uint32_t width = 0, height = 0;
  BLB_RendererGetViewport(renderer, &width, &height);

  BLB_Texture *texture = BLB_Object2D_GetTexture(object);

  BLB_RendererDrawInstancedPolygon2D(renderer, polygon, (float)width, (float)height, &material, texture, &instance, 1,
                                     object->camera_attached);
}

static void draw_object2d(BLB_Object2D *object, BLB_Renderer *renderer, const BLB_CameraCache *camera_cache) {
  if (!object || !renderer || !object->visible || !active_polygon2d(object))
    return;

  BLB_RenderMaterialState state = material_state_from_2d(object);

  draw_object2d_pass(object, renderer, camera_cache, &state, 1.0f, 1.0f);

  if (state.glow <= 0.0f || state.glow_radius <= 0.0f)
    return;

  const int steps = BLB_GLOW_STEPS_2D > 0 ? BLB_GLOW_STEPS_2D : 1;
  const float falloff = fmaxf(state.glow_falloff, 0.2f);

  for (int i = 1; i <= steps; i++) {
    const float t = (float)i / (float)steps;
    const float envelope = powf(fmaxf(0.0f, 1.0f - t), falloff);
    const float smooth_envelope = envelope * (0.92f + 0.08f * (1.0f - t));
    const float scale_mul = 1.0f + state.glow_radius * 0.016f * t;
    const float glow_mul = smooth_envelope * state.glow * 0.16f;

    draw_object2d_pass(object, renderer, camera_cache, &state, scale_mul, glow_mul);
  }
}

static void draw_text2d(BLB_Text2D *text, BLB_Renderer *renderer, BLB_Camera *camera, float aspect) {
  if (!text || !renderer || !text->visible || !text->text || !text->font_path || !text->font_loaded)
    return;

  (void)aspect;

  if (!text->camera_attached && (!camera || !camera->camera_cache || !camera->camera_cache->valid))
    return;

  if (BLB_RendererLoadFont(renderer, &text->font) != 0)
    return;

  BLB_RenderMaterialState state = material_state_from_text(text);
  float glyph_scale = text->font.size > 0 ? text->size / (float)text->font.size : 1.0f;

  if (glyph_scale <= 0.0f)
    glyph_scale = 0.001f;

  uint32_t width = 0, height = 0;
  BLB_RendererGetViewport(renderer, &width, &height);
  BLB_Mat4 matrix = text->camera_attached ? BLB_CameraPixelProjection((float)width, (float)height)
                                          : camera->camera_cache->view_projection;
  BLB_Vec2 scale = text->scale;

  if (!text->camera_attached)
    scale.y = -scale.y;

  BLB_RendererDrawTextTransformed(renderer, &text->font, text->text, text->position.x, text->position.y, glyph_scale,
                                  scale, text->rotation, state.base_color[0], state.base_color[1], state.base_color[2],
                                  state.base_color[3], state.emission, state.glow, state.roughness,
                                  normalize_render_mode(state.render_mode), &matrix.Elements[0][0]);

  printf("r = %f\ng = %f\nb = %f\na = %f\n", state.base_color[0], state.base_color[1], state.base_color[2],
         state.base_color[3]);
}

static uint64_t render_sort_signature3d(BLB_Object3D **objects, int count) {
  uint64_t h = UINT64_C(1469598103934665603);

  for (int i = 0; i < count; ++i) {
    uintptr_t p = (uintptr_t)objects[i];
    BLB_Object3D *o = objects[i];
    uint64_t v = (uint64_t)p ^ ((uint64_t)(o ? (uint32_t)(o->layer + 32768) : 0u) << 32u) ^
                 (uint64_t)(o ? (uint32_t)object3d_render_mode(o) : 0u);
    h ^= v;
    h *= UINT64_C(1099511628211);
  }

  return h ^ (uint64_t)(unsigned)count;
}

static uint64_t render_sort_signature2d(BLB_Object2D **objects, int count) {
  uint64_t h = UINT64_C(1469598103934665603);

  for (int i = 0; i < count; ++i) {
    uintptr_t p = (uintptr_t)objects[i];
    BLB_Object2D *o = objects[i];
    uint64_t v = (uint64_t)p ^ ((uint64_t)(o ? (uint32_t)(o->layer + 32768) : 0u) << 32u) ^
                 (uint64_t)(o ? (uint32_t)object2d_render_mode(o) : 0u);
    h ^= v;
    h *= UINT64_C(1099511628211);
  }

  return h ^ (uint64_t)(unsigned)count;
}

static uint64_t render_sort_signature_text2d(BLB_Text2D **objects, int count) {
  uint64_t h = UINT64_C(1469598103934665603);

  for (int i = 0; i < count; ++i) {
    uintptr_t p = (uintptr_t)objects[i];
    BLB_Text2D *o = objects[i];
    uint64_t v = (uint64_t)p ^ ((uint64_t)(o ? (uint32_t)(o->layer + 32768) : 0u) << 32u) ^
                 (uint64_t)(o ? (uint32_t)o->render_mode : 0u);
    h ^= v;
    h *= UINT64_C(1099511628211);
  }

  return h ^ (uint64_t)(unsigned)count;
}

static uint64_t render_sort_signature_light3d(BLB_Light3D **lights, int count) {
  uint64_t h = UINT64_C(1469598103934665603);

  for (int i = 0; i < count; ++i) {
    uintptr_t p = (uintptr_t)lights[i];
    BLB_Light3D *l = lights[i];
    uint64_t v = (uint64_t)p ^ ((uint64_t)(l && l->object ? (uint32_t)(l->object->layer + 32768) : 0u) << 32u) ^
                 (uint64_t)(l && l->object ? (uint32_t)l->object->render_mode : 0u);
    h ^= v;
    h *= UINT64_C(1099511628211);
  }

  return h ^ (uint64_t)(unsigned)count;
}

static uint64_t render_sort_signature_light2d(BLB_Light2D **lights, int count) {
  uint64_t h = UINT64_C(1469598103934665603);

  for (int i = 0; i < count; ++i) {
    uintptr_t p = (uintptr_t)lights[i];
    BLB_Light2D *l = lights[i];
    uint64_t v = (uint64_t)p ^ ((uint64_t)(l && l->object ? (uint32_t)(l->object->layer + 32768) : 0u) << 32u) ^
                 (uint64_t)(l && l->object ? (uint32_t)l->object->render_mode : 0u);
    h ^= v;
    h *= UINT64_C(1099511628211);
  }

  return h ^ (uint64_t)(unsigned)count;
}

static void prepare_instance_cull_blocks(BLB_RenderInstances3DGroup *group) {
  if (!BLB_OBJECT_BLOCK_CULLING || !group || (!group->cull_blocks_dirty && group->cull_blocks))
    return;

  size_t count = (group->object_count + 63u) / 64u;

  if (count != group->cull_block_count || !group->cull_blocks) {
    BLB_InstanceCullBlock *blocks = realloc(group->cull_blocks, count * sizeof(*blocks));

    if (!blocks)
      return;

    group->cull_blocks = blocks;
    group->cull_block_count = count;
  }

  for (size_t block = 0; block < count; ++block) {
    BLB_Vec3 low = BLB_V3(FLT_MAX, FLT_MAX, FLT_MAX);
    BLB_Vec3 high = BLB_V3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    size_t limit = (block + 1u) * 64u;

    if (limit > group->object_count)
      limit = group->object_count;

    for (size_t i = block * 64u; i < limit; ++i) {
      BLB_Object3D *object = group->objects[i];

      if (!object)
        continue;

      float scale = fmaxf(fabsf(object->scale.x), fmaxf(fabsf(object->scale.y), fabsf(object->scale.z)));
      float radius = object_local_radius(object) * scale;

      for (int axis = 0; axis < 3; ++axis) {
        low.Elements[axis] = fminf(low.Elements[axis], object->position.Elements[axis] - radius);
        high.Elements[axis] = fmaxf(high.Elements[axis], object->position.Elements[axis] + radius);
      }
    }

    group->cull_blocks[block].center = BLB_MulV3F(BLB_AddV3(low, high), 0.5f);
    group->cull_blocks[block].radius = BLB_LenV3(BLB_MulV3F(BLB_SubV3(high, low), 0.5f));
  }
  group->cull_blocks_dirty = false;
}

static bool instance_cull_block_visible(BLB_RenderInstances3DGroup *group, size_t index,
                                        const BLB_CameraCache *camera_cache, BLB_Vec3 camera_position,
                                        const BLB_ShadowFrustum *shadow) {

  if (!BLB_OBJECT_BLOCK_CULLING || !BLB_OBJECT_CULLING || !group->cull_blocks || group->cull_blocks_dirty ||
      index / 64u >= group->cull_block_count)
    return true;

  BLB_InstanceCullBlock *block = &group->cull_blocks[index / 64u];

  if (shadow) {
    if (BLB_OBJECT_SHADOW_CULL_BY_CAMERA && BLB_OBJECT_MAX_SHADOW_RENDER_DISTANCE > 0.0f) {
      BLB_Vec3 delta = BLB_SubV3(block->center, camera_position);
      float distance = BLB_OBJECT_MAX_SHADOW_RENDER_DISTANCE + block->radius;

      if (BLB_DotV3(delta, delta) > distance * distance)
        return false;
    }

    return shadow_sphere_visible(shadow, block->center, block->radius * 1.05f);
  }

  if (BLB_OBJECT_DISTANCE_CULLING && BLB_OBJECT_MAX_RENDER_DISTANCE > 0.0f) {
    BLB_Vec3 delta = BLB_SubV3(block->center, camera_position);
    float distance = BLB_OBJECT_MAX_RENDER_DISTANCE + block->radius;

    if (BLB_DotV3(delta, delta) > distance * distance)
      return false;
  }

  return !BLB_OBJECT_FRUSTUM_CULLING ||
         view_space_sphere_visible(camera_cache, block->center, block->radius, BLB_OBJECT_CULLING_MARGIN);
}

static void draw_instanced_group3d(BLB_RenderInstances3DGroup *group, BLB_Renderer *renderer,
                                   const BLB_CameraCache *camera_cache, BLB_Scene *scene) {
  if (!group || !group->enabled || group->object_count == 0 || !renderer || !camera_cache || !camera_cache->valid ||
      !scene || !scene->camera)
    return;

  prepare_instance_cull_blocks(group);
  BLB_Object3D *source = NULL;

  for (size_t i = 0; i < group->object_count; ++i) {
    if (group->objects[i] && group->objects[i]->visible && !group->objects[i]->camera_attached) {
      source = group->objects[i];
      break;
    }
  }

  if (!source || !source->polygon || !source->material)
    return;

  if (group->bounds_valid && !scene->optimization_rebuild_pending && BLB_OBJECT_CULLING) {
    if (BLB_OBJECT_DISTANCE_CULLING && BLB_OBJECT_MAX_RENDER_DISTANCE > 0.0f) {
      BLB_Vec3 delta = BLB_SubV3(group->bounds_center, scene->camera->position);
      float max_distance = BLB_OBJECT_MAX_RENDER_DISTANCE + group->bounds_radius;

      if (BLB_DotV3(delta, delta) > max_distance * max_distance)
        return;
    }

    if (BLB_OBJECT_FRUSTUM_CULLING &&
        !view_space_sphere_visible(camera_cache, group->bounds_center, group->bounds_radius, BLB_OBJECT_CULLING_MARGIN))
      return;
  }

  size_t max_batch = BLB_OBJECT_INSTANCING_MAX_INSTANCES > 0 ? (size_t)BLB_OBJECT_INSTANCING_MAX_INSTANCES : 1u;

  if (max_batch > group->object_count)
    max_batch = group->object_count;

  if (!instance_group_reserve(group, max_batch))
    return;

  BLB_RenderMaterialState state = material_state_from_3d(source);
  BLB_RenderMaterial material = renderer_material_from_state(&state, true);
  renderer_material_set_bounds_3d(&material, source->polygon);
  material.base_texture_override = source->texture;

  BLB_Mat4 identity = BLB_M4D(1.0f);
  BLB_Mat4 mvp = BLB_MulM4(camera_cache->projection, BLB_MulM4(camera_cache->view, identity));

  BLB_RenderInstanceData *batch = group->render_batch;

  if (!BLB_OBJECT_LOD || BLB_OBJECT_LOD_LEVELS <= 0 ||
      (BLB_OBJECT_LOD_MIN_TRIANGLES > 0 && source->polygon->index_count / 3u <= (size_t)BLB_OBJECT_LOD_MIN_TRIANGLES)) {

    Mesh scratch = {0};
    const Mesh *mesh = object_geometry_mesh(source, &scratch);

    if (!mesh)
      return;

    size_t count = 0;

    for (size_t i = 0; i < group->object_count; ++i) {
      if (i % 64u == 0 && !instance_cull_block_visible(group, i, camera_cache, scene->camera->position, NULL)) {
        i += 63u;
        continue;
      }

      BLB_Object3D *object = group->objects[i];

      if (!object || !object->visible || object->camera_attached)
        continue;

      BLB_RenderObjects3DCluster *cluster = object->optimization_cluster;

      if (BLB_OBJECT_AGGREGATION && cluster && !cluster->dirty && !object_should_use_instance_path(object))
        continue;

      if (!cached_object3d_visible(scene->visibility_cache, object, camera_cache, scene->camera->position))
        continue;

      append_cached_instance(&batch[count++], group, i);
      ++group->visible_instances;

      if (count == max_batch) {
        BLB_RendererDrawInstancedMesh(renderer, mesh, &mvp.Elements[0][0], &material, source->texture, batch, count,
                                      scene->camera->position);
        ++group->draw_calls;
        count = 0;
      }
    }

    if (count) {
      BLB_RendererDrawInstancedMesh(renderer, mesh, &mvp.Elements[0][0], &material, source->texture, batch, count,
                                    scene->camera->position);
      ++group->draw_calls;
    }

    return;
  }

  if (group->render_levels_capacity < group->object_count) {
    uint8_t *levels = realloc(group->render_levels, group->object_count);

    if (!levels)
      return;

    group->render_levels = levels;
    group->render_levels_capacity = group->object_count;
  }

  uint8_t *resolved_levels = group->render_levels;
  memset(resolved_levels, 0, group->object_count);

  int max_lod = BLB_OBJECT_LOD ? (BLB_OBJECT_LOD_LEVELS > 0 ? BLB_OBJECT_LOD_LEVELS : 0) : 0;
  size_t level_counts[32] = {0};
  int max_requested_level = 0;
  int available_levels[32];

  for (int i = 0; i < 32; ++i)
    available_levels[i] = -1;

  if (max_lod > 31)
    max_lod = 31;

  for (size_t i = 0; i < group->object_count; ++i) {
    if (i % 64u == 0 && !instance_cull_block_visible(group, i, camera_cache, scene->camera->position, NULL)) {
      i += 63u;
      continue;
    }

    BLB_Object3D *object = group->objects[i];

    if (!object || !object->visible || object->camera_attached)
      continue;

    bool dynamic = object_should_use_instance_path(object);
    BLB_RenderObjects3DCluster *cluster = object->optimization_cluster;

    if (BLB_OBJECT_AGGREGATION && cluster && !cluster->dirty && !dynamic)
      continue;

    if (!cached_object3d_visible(scene->visibility_cache, object, camera_cache, scene->camera->position))
      continue;

    float distance = BLB_LenV3(BLB_SubV3(object->position, scene->camera->position));
    int requested_level = resolve_screen_lod3d(object, camera_cache, distance);

    if (requested_level > max_requested_level) {
      BLB_Object3D_EnsureLODLevel(source, requested_level);
      max_requested_level = requested_level;
    }

    if (requested_level < 0)
      requested_level = 0;

    if (requested_level > max_lod)
      requested_level = max_lod;

    if (available_levels[requested_level] < 0)
      available_levels[requested_level] = BLB_Object3D_GetAvailableLODLevel(source, requested_level);

    int actual_level = available_levels[requested_level];

    if (actual_level < 0)
      actual_level = 0;

    if (actual_level > max_lod)
      actual_level = max_lod;

    resolved_levels[i] = (uint8_t)(actual_level + 1);
    ++level_counts[actual_level];
    ++group->visible_instances;
  }

  for (int level = 0; level <= max_lod; ++level) {
    if (level_counts[level] == 0)
      continue;

    const BLB_Polygon3D *polygon = level > 0 ? BLB_Object3D_GetLODPolygonAtLevel(source, level) : source->polygon;
    Mesh source_mesh;
    memset(&source_mesh, 0, sizeof(source_mesh));
    const Mesh *mesh = level == 0 ? object_geometry_mesh(source, &source_mesh) : NULL;

    if ((level == 0 && !mesh) || (level > 0 && !polygon))
      continue;

    size_t count = 0;

    for (size_t i = 0; i < group->object_count; ++i) {
      if (i % 64u == 0 && !instance_cull_block_visible(group, i, camera_cache, scene->camera->position, NULL)) {
        i += 63u;
        continue;
      }

      if (resolved_levels[i] != (uint8_t)(level + 1))
        continue;

      BLB_Object3D *object = group->objects[i];

      if (!object)
        continue;

      append_cached_instance(&batch[count++], group, i);

      if (count == max_batch) {
        ++group->draw_calls;

        if (level == 0)
          BLB_RendererDrawInstancedMesh(renderer, mesh, &mvp.Elements[0][0], &material, source->texture, batch, count,
                                        scene->camera->position);
        else
          BLB_RendererDrawInstancedPolygon3D(renderer, polygon, &mvp.Elements[0][0], &material, source->texture, batch,
                                             count, scene->camera->position);
        count = 0;
      }
    }

    if (count > 0) {
      ++group->draw_calls;

      if (level == 0)
        BLB_RendererDrawInstancedMesh(renderer, mesh, &mvp.Elements[0][0], &material, source->texture, batch, count,
                                      scene->camera->position);
      else
        BLB_RendererDrawInstancedPolygon3D(renderer, polygon, &mvp.Elements[0][0], &material, source->texture, batch,
                                           count, scene->camera->position);
    }
  }
}

static void draw_instanced_group3d_shadow(BLB_RenderInstances3DGroup *group, BLB_Renderer *renderer,
                                          const BLB_ShadowFrustum *shadow_frustum, const BLB_Mat4 *shadow_vp,
                                          BLB_Vec3 camera_position) {
  if (!group || !group->enabled || group->object_count == 0 || !renderer || !shadow_frustum || !shadow_vp)
    return;

  prepare_instance_cull_blocks(group);
  BLB_Object3D *source = NULL;

  for (size_t i = 0; i < group->object_count; ++i) {
    if (group->objects[i] && group->objects[i]->visible && !group->objects[i]->camera_attached) {
      source = group->objects[i];
      break;
    }
  }

  if (!source || !source->polygon)
    return;

  if (group->bounds_valid && BLB_OBJECT_CULLING &&
      !shadow_sphere_visible(shadow_frustum, group->bounds_center, group->bounds_radius * 1.05f))
    return;

  int max_lod = BLB_OBJECT_LOD && BLB_OBJECT_LOD_LEVELS > 0 ? BLB_OBJECT_LOD_LEVELS : 0;
  int shadow_level = BLB_OBJECT_LOD_SHADOW_MAX_LEVEL > 0 ? BLB_OBJECT_LOD_SHADOW_MAX_LEVEL : 0;

  if (shadow_level > max_lod)
    shadow_level = max_lod;

  if (shadow_level > 31)
    shadow_level = 31;

  if (shadow_level > 0 && BLB_OBJECT_LOD_MIN_TRIANGLES > 0 &&
      source->polygon->index_count / 3u <= (size_t)BLB_OBJECT_LOD_MIN_TRIANGLES)
    shadow_level = 0;

  if (shadow_level > 0)
    BLB_Object3D_EnsureLODLevel(source, shadow_level);

  const BLB_Polygon3D *polygon =
      shadow_level > 0 ? BLB_Object3D_GetLODPolygonAtLevel(source, shadow_level) : source->polygon;

  if (!polygon)
    return;

  size_t max_batch = (size_t)(BLB_OBJECT_INSTANCING_MAX_INSTANCES > 0 ? BLB_OBJECT_INSTANCING_MAX_INSTANCES : 1);

  if (max_batch > group->object_count)
    max_batch = group->object_count;

  if (!instance_group_reserve(group, max_batch))
    return;

  BLB_RenderInstanceData *batch = group->render_batch;

  size_t count = 0;

  for (size_t i = 0; i < group->object_count; ++i) {
    if (i % 64u == 0 && !instance_cull_block_visible(group, i, NULL, camera_position, shadow_frustum)) {
      i += 63u;
      continue;
    }

    BLB_Object3D *object = group->objects[i];

    if (!object || !object->visible || object->camera_attached)
      continue;

    bool dynamic = object_should_use_instance_path(object);

    if (BLB_OBJECT_AGGREGATION && object->optimization_cluster && !object->optimization_cluster->dirty && !dynamic)
      continue;

    float local_radius = object_local_radius(object);
    float scale_x = fabsf(object->scale.x);
    float scale_y = fabsf(object->scale.y);
    float scale_z = fabsf(object->scale.z);
    float radius = local_radius * fmaxf(scale_x, fmaxf(scale_y, scale_z));

    if (BLB_OBJECT_SHADOW_CULL_BY_CAMERA && BLB_OBJECT_MAX_SHADOW_RENDER_DISTANCE > 0.0f) {
      BLB_Vec3 delta = BLB_SubV3(object->position, camera_position);
      float max_distance = BLB_OBJECT_MAX_SHADOW_RENDER_DISTANCE + radius;

      if (BLB_DotV3(delta, delta) > max_distance * max_distance)
        continue;
    }

    if (!shadow_sphere_visible(shadow_frustum, object->position, radius * 1.05f))
      continue;

    if (count == max_batch) {
      BLB_RendererDrawInstancedShadowPolygon3D(renderer, polygon, &shadow_vp->Elements[0][0], batch, count);
      count = 0;
    }
    append_cached_instance(&batch[count++], group, i);
  }

  if (count > 0)
    BLB_RendererDrawInstancedShadowPolygon3D(renderer, polygon, &shadow_vp->Elements[0][0], batch, count);
}

static int select_aggregated_lod3d(const BLB_RenderObjects3DCluster *cluster, BLB_Vec3 center,
                                   BLB_Vec3 camera_position) {
  if (!cluster || !cluster->lods || cluster->lod_count == 0 || !BLB_OBJECT_LOD)
    return 0;

  float distance = BLB_LenV3(BLB_SubV3(camera_position, center));
  int max_level = BLB_OBJECT_LOD_LEVELS > 0 ? BLB_OBJECT_LOD_LEVELS : 0;
  int level = 0;

  if (BLB_OBJECT_LOD_DISTANCE_OPTIMIZATION && BLB_OBJECT_LOD_DISTANCE_STEP > 0.0f &&
      distance > BLB_OBJECT_LOD_DISTANCE_START) {
    int extra = 1 + (int)floorf((distance - BLB_OBJECT_LOD_DISTANCE_START) / BLB_OBJECT_LOD_DISTANCE_STEP);
    level = BLB_OBJECT_LOD_DEFAULT_LEVEL + extra;
  }

  if (level > max_level)
    level = max_level;

  int minimum = BLB_OBJECT_LOD_MIN_LEVEL;

  if (minimum < 0)
    minimum = 0;

  if (level < minimum)
    level = minimum;

  if (level > max_level)
    level = max_level;

  return level;
}

static bool cluster_has_camera_attached_object(const BLB_RenderObjects3DCluster *cluster) {
  if (!cluster || !cluster->objects)
    return false;

  for (size_t i = 0; i < cluster->object_count; ++i) {
    if (cluster->objects[i] && cluster->objects[i]->camera_attached)
      return true;
  }

  return false;
}

static void draw_aggregated_cluster3d(BLB_RenderObjects3DCluster *cluster, BLB_Renderer *renderer,
                                      const BLB_CameraCache *camera_cache, BLB_Scene *scene) {
  if (!cluster || cluster->dirty || !cluster->mesh || cluster_has_camera_attached_object(cluster) || !renderer ||
      !camera_cache || !camera_cache->valid || !scene || !scene->camera || cluster->object_count == 0)
    return;

  if (!cluster_visible_to_camera(cluster, camera_cache, scene->camera->position))
    return;

  BLB_Vec3 center = BLB_MulV3F(BLB_AddV3(cluster->bounds->min, cluster->bounds->max), 0.5f);
  int lod = select_aggregated_lod3d(cluster, center, scene->camera->position);

  Mesh *mesh = BLB_ObjectAggregation3D_GetLOD(cluster, (size_t)lod);

  if (!mesh || !mesh->vertices || !mesh->indices || mesh->vertex_count == 0 || mesh->index_count < 3)
    return;

  BLB_Object3D *source = cluster->objects[0];

  if (!source || !source->material)
    return;

  BLB_RenderMaterialState state = material_state_from_3d(source);
  BLB_RenderMaterial material = renderer_material_from_state(&state, true);
  material.base_texture_override = source->texture;

  BLB_Mat4 identity = BLB_M4D(1.0f);
  BLB_Mat4 mvp = BLB_MulM4(camera_cache->projection, BLB_MulM4(camera_cache->view, identity));

  BLB_RendererDrawCachedMeshMaterial(renderer, mesh, &mvp.Elements[0][0], NULL, state.base_color[0],
                                     state.base_color[1], state.base_color[2], state.base_color[3], &material,
                                     source->texture, scene->camera->position);
}

typedef struct {
  BLB_Scene *scene;
  BLB_Object3D **originals;
  BLB_Object3D **snapshots;
  size_t count;
  float cluster_distance;
  int hsa_levels;
  float hsa_error;
  uint64_t generation;
  BLB_ObjectAggregation3D *result;
} BLB_AsyncAggregationJob;

static uint64_t render_object3d_generation(BLB_Object3D **objects, size_t count) {
  uint64_t hash = UINT64_C(1469598103934665603);

  for (size_t i = 0; i < count; ++i) {
    BLB_Object3D *object = objects ? objects[i] : NULL;
    uintptr_t values[7] = {
        (uintptr_t)object,
        (uintptr_t)(object ? object->polygon : NULL),
        (uintptr_t)(object ? object->material : NULL),
        (uintptr_t)(object ? object->texture : NULL),
        object ? object->geometry_id : 0,
        object ? object->transform_revision : 0,
        object ? (uintptr_t)object->camera_attached : 0,
    };

    for (size_t j = 0; j < 7; ++j) {
      hash ^= (uint64_t)values[j];
      hash *= UINT64_C(1099511628211);
    }

    if (object) {
      const float values[] = {object->position.x, object->position.y, object->position.z, object->rotation.x,
                              object->rotation.y, object->rotation.z, object->scale.x,    object->scale.y,
                              object->scale.z,    object->color.x,    object->color.y,    object->color.z,
                              object->color.w};

      for (size_t k = 0; k < sizeof(values) / sizeof(values[0]); ++k) {
        uint32_t bits;
        memcpy(&bits, &values[k], sizeof(bits));
        hash ^= bits;
        hash *= UINT64_C(1099511628211);
      }
      hash ^= (uint64_t)object->layer | ((uint64_t)object->visible << 32);
      hash *= UINT64_C(1099511628211);
    }

    if (object && object->material) {
      hash ^= object->material->revision;
      hash *= UINT64_C(1099511628211);
    }
  }

  return hash ^ (uint64_t)count;
}

static void async_aggregation_run(void *user_data) {
  BLB_AsyncAggregationJob *job = user_data;

  if (!job || !job->snapshots || job->count == 0)
    return;
  job->result = BLB_ObjectAggregation3D_Create(job->snapshots, job->count, job->cluster_distance, job->hsa_levels,
                                               job->hsa_error);

  if (job->result && BLB_OBJECT_LOD && BLB_OBJECT_LOD_LEVELS > 0) {
    size_t target_level = (size_t)BLB_OBJECT_LOD_LEVELS;

    for (size_t i = 0; i < job->result->cluster_count; ++i) {
      BLB_RenderObjects3DCluster *cluster = job->result->clusters[i];

      if (cluster)
        BLB_ObjectAggregation3D_EnsureLOD(job->result, cluster, target_level);
    }
  }
}

static void async_aggregation_complete(void *user_data) {
  BLB_AsyncAggregationJob *job = user_data;

  if (!job || !job->scene)
    return;

  job->scene->optimization_async_pending = false;

  if (!job->result)
    return;

  if (render_object3d_generation(job->scene->objects3d, (size_t)job->scene->object3d_count) != job->generation ||
      job->scene->object3d_count != (int)job->count ||
      !BLB_ObjectAggregation3D_RebindSource(job->result, job->originals, job->snapshots, job->count)) {
    BLB_ObjectAggregation3D_Destroy(job->result);
    job->result = NULL;
    job->scene->optimization_rebuild_pending = true;

    return;
  }

  job->scene->optimization_async_result = job->result;
  job->scene->optimization_async_generation = job->generation;
  job->result = NULL;
}

static void async_aggregation_destroy(void *user_data) {
  BLB_AsyncAggregationJob *job = user_data;

  if (!job)
    return;
  BLB_ObjectAggregation3D_Destroy(job->result);
  free(job->originals);

  if (job->snapshots) {
    for (size_t i = 0; i < job->count; ++i)
      free(job->snapshots[i]);
  }
  free(job->snapshots);
  free(job);
}

static bool schedule_async_aggregation(BLB_Scene *scene) {
  if (!scene || scene->object3d_count <= 0 || scene->optimization_async_pending || !BLB_OBJECT_AGGREGATION)
    return false;

  size_t count = (size_t)scene->object3d_count;
  BLB_AsyncAggregationJob *job = calloc(1, sizeof(*job));

  if (!job)
    return false;

  job->scene = scene;
  job->count = count;
  job->cluster_distance = BLB_OBJECT_AGGREGATION_MAX_DISTANCE;
  job->hsa_levels = BLB_HSA ? BLB_HSA_LEVELS : 0;
  job->hsa_error = BLB_HSA_ERROR;
  job->generation = render_object3d_generation(scene->objects3d, count);

  job->originals = malloc(count * sizeof(*job->originals));
  job->snapshots = calloc(count, sizeof(*job->snapshots));

  if (!job->originals || !job->snapshots) {
    async_aggregation_destroy(job);

    return false;
  }

  for (size_t i = 0; i < count; ++i) {
    BLB_Object3D *source = scene->objects3d[i];
    job->originals[i] = source;

    if (!source)
      continue;
    job->snapshots[i] = malloc(sizeof(*job->snapshots[i]));

    if (!job->snapshots[i]) {
      async_aggregation_destroy(job);

      return false;
    }
    *job->snapshots[i] = *source;
    job->snapshots[i]->optimization_cluster = NULL;
    job->snapshots[i]->optimization_instance_group = NULL;
  }

  if (BLB_RenderAsync_Submit(async_aggregation_run, async_aggregation_complete, async_aggregation_destroy, job) != 0) {
    async_aggregation_destroy(job);

    return false;
  }

  scene->optimization_async_pending = true;

  return true;
}

static void apply_async_aggregation(BLB_Scene *scene, BLB_Renderer *renderer) {
  if (!scene || !renderer || !scene->optimization_async_result)
    return;

  BLB_ObjectAggregation3D *result = scene->optimization_async_result;

  if (scene->optimization_async_generation !=
      render_object3d_generation(scene->objects3d, (size_t)scene->object3d_count)) {
    BLB_ObjectAggregation3D_Destroy(result);
    scene->optimization_async_result = NULL;
    scene->optimization_rebuild_pending = true;

    return;
  }

  bool old_has_built_clusters = scene->object_aggregation3d && scene->object_aggregation3d->cluster_count > 0;
  bool new_has_built_clusters = result->cluster_count > 0;

  if (old_has_built_clusters || new_has_built_clusters)
    BLB_RendererClearCachedMeshes(renderer);
  BLB_ObjectAggregation3D_Destroy(scene->object_aggregation3d);
  scene->object_aggregation3d = result;
  scene->optimization_async_result = NULL;
  scene->optimization_rebuild_pending = false;
}

// audio helpers functions
static void audio3d_play(BLB_Audio3D *audio3d) {
  if (!audio3d || !audio3d->sound)
    return;

  *audio3d->result = ma_sound_start(audio3d->sound);
  if (*audio3d->result == MA_SUCCESS)
    audio3d->play = true;
}

static void audio2d_play(BLB_Audio2D *audio2d) {
  if (!audio2d || !audio2d->sound)
    return;

  *audio2d->result = ma_sound_start(audio2d->sound);
  if (*audio2d->result == MA_SUCCESS)
    audio2d->play = true;
}

static void audio3d_surround_play(BLB_Audio3D *audio3d, BLB_Scene *scene) {
  if (!audio3d || !audio3d->sound || !scene || !scene->camera)
    return;

  ma_engine *engine = &audio3d->io_audio->engine;

  ma_engine_listener_set_position(engine, 0, scene->camera->position.x, scene->camera->position.y,
                                  scene->camera->position.z);
  float pitch = scene->camera->rotation.x * BLB_PI / 180.0f;
  float yaw = scene->camera->rotation.y * BLB_PI / 180.0f;
  BLB_Vec3 forward = BLB_V3(cosf(pitch) * sinf(yaw), -sinf(pitch), -cosf(pitch) * cosf(yaw));
  BLB_Vec3 up = BLB_V3(sinf(yaw) * sinf(pitch), cosf(pitch), -cosf(yaw) * sinf(pitch));
  ma_engine_listener_set_world_up(engine, 0, up.x, up.y, up.z);
  ma_engine_listener_set_direction(engine, 0, forward.x, forward.y, forward.z);

  if (!ma_sound_is_playing(audio3d->sound)) {
    *audio3d->result = ma_sound_start(audio3d->sound);

    if (*audio3d->result == MA_SUCCESS)
      audio3d->play = true;
  }
}

static void audio2d_surround_play(BLB_Audio2D *audio2d, BLB_Scene *scene) {
  if (!audio2d || !audio2d->sound || !scene || !scene->camera)
    return;

  ma_engine_listener_set_position(&audio2d->io_audio->engine, 0, scene->camera->position.x, scene->camera->position.y,
                                  scene->camera->position.z);
  float pitch = scene->camera->rotation.x * BLB_PI / 180.0f;
  float yaw = scene->camera->rotation.y * BLB_PI / 180.0f;
  BLB_Vec3 forward = BLB_V3(cosf(pitch) * sinf(yaw), -sinf(pitch), -cosf(pitch) * cosf(yaw));
  BLB_Vec3 up = BLB_V3(sinf(yaw) * sinf(pitch), cosf(pitch), -cosf(yaw) * sinf(pitch));
  ma_engine_listener_set_world_up(&audio2d->io_audio->engine, 0, up.x, up.y, up.z);
  ma_engine_listener_set_direction(&audio2d->io_audio->engine, 0, forward.x, forward.y, forward.z);

  *audio2d->result = ma_sound_start(audio2d->sound);
  if (*audio2d->result == MA_SUCCESS)
    audio2d->play = true;
}

// helpers in BLB_DrawScene
static void debug_aggregation(BLB_Scene *scene) {
  if (!BLB_DEBUG)
    return;

  if (!scene) {
    fprintf(stderr, "[BLB:Render] aggregation=NULL\n");

    return;
  }

  if (!scene->object_aggregation3d) {
    fprintf(stderr, "[BLB:Render] aggregation=OFF objects=%d\n", scene->object3d_count);

    return;
  }

  size_t aggregated_objects = 0;
  size_t instanced_objects = 0;
  size_t enabled_instance_groups = 0;
  size_t visible_instances = 0;
  size_t instance_draws = 0;
  size_t max_cluster_size = 0;
  size_t lod0_vertices = 0;
  size_t lod0_indices = 0;
  size_t last_indices = 0;
  size_t dynamic_fallback_objects = 0;

  for (size_t i = 0; i < scene->object_aggregation3d->cluster_count; i++) {
    BLB_RenderObjects3DCluster *cluster = scene->object_aggregation3d->clusters[i];

    if (!cluster)
      continue;

    aggregated_objects += cluster->object_count;

    if (cluster->object_count > max_cluster_size)
      max_cluster_size = cluster->object_count;

    if (cluster->mesh) {
      lod0_vertices += cluster->mesh->vertex_count;
      lod0_indices += cluster->mesh->index_count;
    }

    if (cluster->lod_count > 0 && cluster->lods[cluster->lod_count - 1])
      last_indices += cluster->lods[cluster->lod_count - 1]->index_count;

    if (!BLB_OBJECT_AGGREGATION_REBUILD_ON_TRANSFORM_CHANGE && BLB_ObjectAggregation3D_ClusterTransformChanged(cluster))
      dynamic_fallback_objects += cluster->object_count;
  }

  for (size_t i = 0; i < scene->object_aggregation3d->instance_group_count; ++i) {
    BLB_RenderInstances3DGroup *group = scene->object_aggregation3d->instance_groups[i];

    if (!group || !group->enabled)
      continue;

    ++enabled_instance_groups;
    visible_instances += group->visible_instances;
    instance_draws += group->draw_calls;
    instanced_objects += group->object_count;
  }

  fprintf(stderr,
          "[BLB:Render] optimization=ON objects=%d clusters=%zu aggregated=%zu "
          "instance_groups=%zu instanced=%zu dynamic_fallback=%zu "
          "max_cluster=%zu "
          "lod_levels=%d lod0_v=%zu lod0_i=%zu last_i=%zu "
          "visible_instances=%zu instance_draws=%zu\n",
          scene->object3d_count, scene->object_aggregation3d->cluster_count, aggregated_objects,
          enabled_instance_groups, instanced_objects, dynamic_fallback_objects, max_cluster_size,
          scene->object_aggregation3d->hsa_levels, lod0_vertices, lod0_indices, last_indices, visible_instances,
          instance_draws);

  size_t groups2d = 0;
  size_t instanced2d = 0;

  if (scene->object_optimization2d) {
    groups2d = scene->object_optimization2d->group_count;

    for (size_t i = 0; i < groups2d; ++i) {
      BLB_RenderInstances2DGroup *group = scene->object_optimization2d->groups[i];

      if (group && group->enabled)
        instanced2d += group->object_count;
    }
  }

  fprintf(stderr, "[BLB:Render] 2d_instancing=%s objects=%d groups=%zu instanced=%zu\n",
          scene->object_optimization2d ? "ON" : "OFF", scene->object2d_count, groups2d, instanced2d);
}

static void update_object3d_delta_time(BLB_Scene *scene) {
  for (int i = 0; i < scene->object3d_count; ++i) {
    BLB_Object3D *object = scene->objects3d[i];

    if (!object)
      continue;

    object->delta_time = scene->delta_time;
    BLB_Model_UpdateAnimation(object, scene->delta_time);
  }
}

static void update_object2d_delta_time(BLB_Scene *scene) {
  for (int i = 0; i < scene->object2d_count; ++i) {
    BLB_Object2D *object = scene->objects2d[i];

    if (!object)
      continue;

    object->delta_time = scene->delta_time;

    BLB_Object2D_UpdateAnimation(object, scene->delta_time);
  }
}

static void update_text2d_delta_time(BLB_Scene *scene) {
  for (int i = 0; i < scene->text2d_count; ++i) {
    BLB_Text2D *text = scene->text2d[i];

    if (text)
      text->delta_time = scene->delta_time;
  }
}

static void update_light3d_delta_time(BLB_Scene *scene) {
  for (int i = 0; i < scene->light3d_count; ++i) {
    BLB_Light3D *light = scene->lights3d[i];

    if (light && light->object)
      light->object->delta_time = scene->delta_time;
  }
}

static void update_light2d_delta_time(BLB_Scene *scene) {
  for (int i = 0; i < scene->light2d_count; ++i) {
    BLB_Light2D *light = scene->lights2d[i];

    if (light && light->object)
      light->object->delta_time = scene->delta_time;
  }
}

static void update_particles3d(BLB_Scene *scene) {
  for (int i = 0; i < scene->particle3d_count; ++i) {
    BLB_Particle3D *particle = scene->particles3d[i];

    if (!particle)
      continue;

    particle->delta_time = scene->delta_time;

    if (!particle->visible)
      continue;

    particle->position = BLB_AddV3(particle->position, BLB_MulV3F(particle->velocity, scene->delta_time));

    particle->rotation = BLB_AddV3(particle->rotation, BLB_MulV3F(particle->rotation_speed, scene->delta_time));

    particle->lifetime += scene->delta_time;

    if (particle->lifetime_max > 0.0f && particle->lifetime >= particle->lifetime_max)
      particle->visible = false;
  }
}

static void update_particles2d(BLB_Scene *scene) {
  for (int i = 0; i < scene->particle2d_count; ++i) {
    BLB_Particle2D *particle = scene->particles2d[i];

    if (!particle)
      continue;

    particle->delta_time = scene->delta_time;

    if (!particle->visible)
      continue;

    particle->position = BLB_AddV2(particle->position, BLB_MulV2F(particle->velocity, scene->delta_time));

    particle->rotation += particle->rotation_speed * scene->delta_time;
    particle->lifetime += scene->delta_time;

    if (particle->lifetime_max > 0.0f && particle->lifetime >= particle->lifetime_max)
      particle->visible = false;
  }
}

static void update_particle_systems3d(BLB_Scene *scene) {
  for (int i = 0; i < scene->particle_system3d_count; ++i) {
    BLB_ParticleSystem3D *system = scene->particle_systems3d[i];

    if (!system)
      continue;

    system->delta_time = scene->delta_time;
    BLB_ParticleSystem3D_Update(system, scene->delta_time);
  }
}

static void update_particle_systems2d(BLB_Scene *scene) {
  for (int i = 0; i < scene->particle_system2d_count; ++i) {
    BLB_ParticleSystem2D *system = scene->particle_systems2d[i];

    if (!system)
      continue;

    system->delta_time = scene->delta_time;
    BLB_ParticleSystem2D_Update(system, scene->delta_time);
  }
}

static void update_scene(BLB_Scene *scene) {
  if (!scene->enabled)
    return;

  update_object3d_delta_time(scene);
  update_object2d_delta_time(scene);
  update_text2d_delta_time(scene);
  update_light3d_delta_time(scene);
  update_light2d_delta_time(scene);
  update_particles3d(scene);
  update_particles2d(scene);
  update_particle_systems3d(scene);
  update_particle_systems2d(scene);
}

static void prepare_clear(BLB_Scene *scene, BLB_Renderer *renderer) {
  if (scene->clear_enabled) {
    BLB_RendererSetClearColor(renderer, scene->clear_color[0] / 255.0f, scene->clear_color[1] / 255.0f,
                              scene->clear_color[2] / 255.0f, scene->clear_color[3] / 255.0f);
  } else {
    BLB_RendererSetClearColor(renderer, 0.0f, 0.0f, 0.0f, 1.0f);
  }
}

static void prepare_optimization(BLB_Scene *scene, BLB_Renderer *renderer) {
  for (int i = 0; i < scene->object3d_count; ++i)
    update_object_motion_state(scene->objects3d[i], scene);
  const bool instancing_active = BLB_OBJECT_INSTANCING && scene->object3d_count >= BLB_OBJECT_INSTANCING_MIN_OBJECTS;
  const bool aggregation_active = BLB_OBJECT_AGGREGATION && scene->object3d_count >= BLB_OBJECT_AGGREGATION_MIN_OBJECTS;
  const bool object_optimization_active = instancing_active || aggregation_active;

  apply_async_aggregation(scene, renderer);

  if (object_optimization_active) {
    bool structure_mismatch = !scene->object_aggregation3d ||
                              scene->object_aggregation3d->source_count != (size_t)scene->object3d_count ||
                              !BLB_ObjectAggregation3D_MatchesSource(scene->object_aggregation3d, scene->objects3d,
                                                                     (size_t)scene->object3d_count);

    if (structure_mismatch) {
      BLB_RendererClearCachedMeshes(renderer);
      BLB_ObjectAggregation3D_Destroy(scene->object_aggregation3d);

      scene->object_aggregation3d = BLB_ObjectAggregation3D_CreateLightweight(
          scene->objects3d, (size_t)scene->object3d_count, BLB_OBJECT_AGGREGATION_MAX_DISTANCE,
          BLB_HSA ? BLB_HSA_LEVELS : 0, BLB_HSA_ERROR);

      scene->optimization_rebuild_pending = false;
    }

    if (scene->optimization_rebuild_pending && scene->object_aggregation3d) {
      if (scene->object_aggregation3d->cluster_count > 0)
        scene->object_aggregation3d->build_complete = false;
      scene->optimization_rebuild_pending = false;
    }

    if (scene->object_aggregation3d && !scene->object_aggregation3d->build_complete &&
        !scene->optimization_async_pending && !scene->optimization_async_result) {
      schedule_async_aggregation(scene);
    }
  } else if (scene->object_aggregation3d) {
    BLB_RendererClearCachedMeshes(renderer);

    BLB_ObjectAggregation3D_Destroy(scene->object_aggregation3d);
    scene->object_aggregation3d = NULL;
  }

  const bool object2d_instancing_active =
      BLB_OBJECT_2D_INSTANCING && scene->object2d_count >= BLB_OBJECT_2D_INSTANCING_MIN_OBJECTS;

  if (object2d_instancing_active) {
    if (!scene->object_optimization2d || !BLB_ObjectOptimization2D_MatchesSource(
                                             scene->object_optimization2d, scene->objects2d, scene->object2d_count)) {

      BLB_ObjectOptimization2D_Destroy(scene->object_optimization2d);

      scene->object_optimization2d = BLB_ObjectOptimization2D_Create(scene->objects2d, (size_t)scene->object2d_count);
    }
  } else if (scene->object_optimization2d) {
    BLB_ObjectOptimization2D_Destroy(scene->object_optimization2d);
    scene->object_optimization2d = NULL;
  }
}

static void prepare_renderer_lighting(BLB_Scene *scene, BLB_Renderer *renderer) {
  if (scene->camera)
    BLB_RendererSetCameraPosition(renderer, scene->camera->position);
  else
    BLB_RendererSetCameraPosition(renderer, BLB_V3(0.0f, 0.0f, 0.0f));

  BLB_RendererSetLights3D(renderer, scene->lights3d, scene->light3d_count);
  BLB_RendererSetLights2D(renderer, scene->lights2d, scene->light2d_count);
  BLB_RendererSetLightingObjects3D(renderer, scene->objects3d, scene->object3d_count);
  BLB_RendererSetLightingObjects2D(renderer, scene->objects2d, scene->object2d_count);

  BLB_RendererUpdateLightBuffer3D(renderer);
  BLB_RendererUpdateLightBuffer2D(renderer);
}

static void prepare_camera(BLB_Scene *scene, BLB_Renderer *renderer, BLB_RenderVisibilityCache *visibility_cache,
                           float viewport_width, float viewport_height) {
  float aspect = viewport_height != 0.0f ? viewport_width / viewport_height : 1.0f;

  if (scene->camera) {
    scene->camera->camera_cache->valid = true;
    scene->camera->camera_cache->viewport_width = viewport_width;
    scene->camera->camera_cache->viewport_height = viewport_height;
    scene->camera->camera_cache->view = BLB_CameraView(scene->camera);
    scene->camera->camera_cache->projection = BLB_CameraProjection(scene->camera, aspect);
    scene->camera->camera_cache->view_projection =
        BLB_MulM4(scene->camera->camera_cache->projection, scene->camera->camera_cache->view);
    BLB_ShadowFrustum frustum;
    shadow_frustum_build(&frustum, &scene->camera->camera_cache->view_projection);
    scene->camera->camera_cache->frustum_valid = true;

    for (size_t i = 0; i < 6; ++i) {
      float length = frustum.lengths[i];

      if (!(length > 0.000001f) || !isfinite(length)) {
        scene->camera->camera_cache->frustum_valid = false;
        break;
      }
      float *plane = scene->camera->camera_cache->frustum_planes[i];
      plane[0] = frustum.normals[i].x / length;
      plane[1] = frustum.normals[i].y / length;
      plane[2] = frustum.normals[i].z / length;
      plane[3] = frustum.offsets[i] / length;
    }
    BLB_RendererSetViewProjection(renderer, &scene->camera->camera_cache->view_projection.Elements[0][0]);

    if (visibility_cache) {
      BLB_RenderVisibilityCache_BeginFrame(visibility_cache, scene->camera->position,
                                           &scene->camera->camera_cache->view_projection, viewport_width,
                                           viewport_height);
      BLB_RenderVisibilityCache_Reserve(visibility_cache,
                                        (size_t)scene->object3d_count + (size_t)scene->object2d_count);
    }
  } else {
    BLB_Mat4 identity = BLB_M4D(1.0f);
    BLB_RendererSetViewProjection(renderer, &identity.Elements[0][0]);
  }
}

static void prepare_sort(BLB_Scene *scene) {
  uint64_t sig3d = render_sort_signature3d(scene->objects3d, scene->object3d_count);
  uint64_t sig2d = render_sort_signature2d(scene->objects2d, scene->object2d_count);
  uint64_t sigt2d = render_sort_signature_text2d(scene->text2d, scene->text2d_count);
  uint64_t sigl3d = render_sort_signature_light3d(scene->lights3d, scene->light3d_count);
  uint64_t sigl2d = render_sort_signature_light2d(scene->lights2d, scene->light2d_count);

  if (scene->object3d_count > 1 && sig3d != scene->sort_signature3d) {
    qsort(scene->objects3d, scene->object3d_count, sizeof(BLB_Object3D *), compare_object3d);
    scene->sort_signature3d = render_sort_signature3d(scene->objects3d, scene->object3d_count);
  }

  if (scene->object3d_count <= 1)
    scene->sort_signature3d = sig3d;

  if (scene->object2d_count > 1 && sig2d != scene->sort_signature2d) {
    qsort(scene->objects2d, scene->object2d_count, sizeof(BLB_Object2D *), compare_object2d);
    scene->sort_signature2d = render_sort_signature2d(scene->objects2d, scene->object2d_count);
  }

  if (scene->object2d_count <= 1)
    scene->sort_signature2d = sig2d;

  if (scene->text2d_count > 1 && sigt2d != scene->sort_signature_text2d) {
    qsort(scene->text2d, scene->text2d_count, sizeof(BLB_Text2D *), compare_text2d);
    scene->sort_signature_text2d = render_sort_signature_text2d(scene->text2d, scene->text2d_count);
  }

  if (scene->text2d_count <= 1)
    scene->sort_signature_text2d = sigt2d;

  if (scene->light3d_count > 1 && sigl3d != scene->sort_signature_light3d) {
    qsort(scene->lights3d, scene->light3d_count, sizeof(BLB_Light3D *), compare_light3d);
    scene->sort_signature_light3d = render_sort_signature_light3d(scene->lights3d, scene->light3d_count);
  }

  if (scene->light3d_count <= 1)
    scene->sort_signature_light3d = sigl3d;

  if (scene->light2d_count > 1 && sigl2d != scene->sort_signature_light2d) {
    qsort(scene->lights2d, scene->light2d_count, sizeof(BLB_Light2D *), compare_light2d);
    scene->sort_signature_light2d = render_sort_signature_light2d(scene->lights2d, scene->light2d_count);
  }

  if (scene->light2d_count <= 1)
    scene->sort_signature_light2d = sigl2d;
}

static int prepare_instance_capacity(BLB_Scene *scene, BLB_Renderer *renderer, uint32_t shadow_mode) {
  size_t required_instance_count = 0;

  for (int i = 0; i < scene->particle_system3d_count; ++i) {
    BLB_ParticleSystem3D *system = scene->particle_systems3d[i];

    if (!system || system->count == 0)
      continue;

    if (system->count > SIZE_MAX - required_instance_count)
      return -1;

    required_instance_count += system->count;
  }

  for (int i = 0; i < scene->particle_system2d_count; ++i) {
    BLB_ParticleSystem2D *system = scene->particle_systems2d[i];

    if (!system || system->count == 0)
      continue;

    if (system->count > SIZE_MAX - required_instance_count)
      return -1;

    required_instance_count += system->count;
  }

  const bool instancing_active = BLB_OBJECT_INSTANCING && scene->object3d_count >= BLB_OBJECT_INSTANCING_MIN_OBJECTS;

  if (instancing_active && scene->object_aggregation3d) {
    size_t shadow_passes = shadow_mode == 2 ? BLB_RENDER_POINT_SHADOW_FACES : (shadow_mode == 1 ? 1u : 0u);
    size_t multiplier = shadow_passes + 1u;

    for (size_t i = 0; i < scene->object_aggregation3d->instance_group_count; ++i) {
      BLB_RenderInstances3DGroup *group = scene->object_aggregation3d->instance_groups[i];

      if (!group || !group->enabled || group->object_count == 0)
        continue;

      if (group->object_count > (SIZE_MAX - required_instance_count) / multiplier)
        return -1;

      required_instance_count += group->object_count * multiplier;
    }
  }

  size_t object2d_passes = 1u + (size_t)(BLB_GLOW_STEPS_2D > 0 ? BLB_GLOW_STEPS_2D : 0);

  if ((size_t)scene->object2d_count > (SIZE_MAX - required_instance_count) / object2d_passes)
    return -1;
  required_instance_count += (size_t)scene->object2d_count * object2d_passes;

  if (required_instance_count == 0)
    return 0;

  return BLB_RendererEnsureInstanceCapacity(renderer, required_instance_count);
}

static uint32_t prepare_shadows(BLB_Scene *scene, BLB_Renderer *renderer, BLB_Mat4 *shadow_vp,
                                BLB_Mat4 point_shadow_mvp[BLB_RENDER_POINT_SHADOW_FACES],
                                BLB_Light3D **point_shadow_light) {

  bool scene_draw_enabled = scene->enabled && scene->visible;
  bool directional_shadow = scene_draw_enabled && build_shadow_matrix(scene, shadow_vp, point_shadow_light);

  bool point_shadow = scene_draw_enabled && !directional_shadow &&
                      build_point_shadow_matrices(scene, point_shadow_mvp, point_shadow_light);

  uint32_t shadow_mode = 0;

  if (directional_shadow) {
    BLB_RendererSetShadow(renderer, &shadow_vp->Elements[0][0], true, 0.003f);
    BLB_RendererSetShadowLight3D(renderer, *point_shadow_light);
  } else if (point_shadow) {
    BLB_RendererSetPointShadow(renderer, point_shadow_mvp, true, 0.002f);
    BLB_RendererSetShadowLight3D(renderer, *point_shadow_light);
  } else {
    BLB_RendererSetShadow(renderer, &shadow_vp->Elements[0][0], false, 0.003f);
  }

  shadow_mode = BLB_RendererGetShadowMode(renderer);

  return shadow_mode;
}

static void draw_shadows(BLB_Scene *scene, BLB_Renderer *renderer, uint32_t shadow_mode) {
  if (shadow_mode == 0)
    return;

  if (!scene->camera)
    return;

  BLB_Mat4 shadow_mvp_cache = BLB_M4D(1.0f);
  uint32_t shadow_count = shadow_mode == 2 ? BLB_RENDER_POINT_SHADOW_FACES : 1;
  uint32_t first_shadow_map = shadow_mode == 2 ? 1u : 0u;

  for (uint32_t pass = 0; pass < shadow_count; ++pass) {
    uint32_t shadow_map_index = first_shadow_map + pass;

    BLB_RendererBeginShadowPass(renderer, shadow_map_index);
    if (!BLB_RendererGetShadowMVP(renderer, shadow_map_index, &shadow_mvp_cache)) {
      BLB_RendererEndShadowPass(renderer);
      continue;
    }

    BLB_ShadowFrustum shadow_frustum;
    shadow_frustum_build(&shadow_frustum, &shadow_mvp_cache);

    if (scene->object_aggregation3d) {
      for (size_t i = 0; i < scene->object_aggregation3d->instance_group_count; ++i) {
        BLB_RenderInstances3DGroup *group = scene->object_aggregation3d->instance_groups[i];

        if (group && group->enabled) {
          draw_instanced_group3d_shadow(group, renderer, &shadow_frustum, &shadow_mvp_cache, scene->camera->position);
        }
      }

      for (size_t i = 0; i < scene->object_aggregation3d->cluster_count; ++i) {
        BLB_RenderObjects3DCluster *cluster = scene->object_aggregation3d->clusters[i];

        if (!cluster || cluster->dirty || !cluster->mesh || cluster_has_camera_attached_object(cluster))
          continue;

        if (cluster->bounds) {
          BLB_Vec3 center = BLB_MulV3F(BLB_AddV3(cluster->bounds->min, cluster->bounds->max), 0.5f);
          BLB_Vec3 extent = BLB_MulV3F(BLB_SubV3(cluster->bounds->max, cluster->bounds->min), 0.5f);
          float radius = BLB_LenV3(extent);

          if (!shadow_sphere_visible(&shadow_frustum, center, radius * 1.05f))
            continue;
        }

        Mesh *shadow_mesh =
            BLB_ObjectAggregation3D_GetLOD(cluster, cluster->lod_count > 0 ? cluster->lod_count - 1 : 0);

        if (!shadow_mesh)
          continue;

        BLB_RendererDrawCachedShadowMesh(renderer, shadow_mesh, &shadow_mvp_cache.Elements[0][0]);
      }
    }

    for (int i = 0; i < scene->object3d_count; ++i) {

      BLB_Object3D *object = scene->objects3d[i];

      if (!object || !object->visible || !active_polygon3d(object) || object->camera_attached)
        continue;

      if (object3d_render_mode(object) != BLB_RENDER_OPAQUE)
        continue;

      if (scene->object_aggregation3d) {
        BLB_RenderObjects3DCluster *cluster = BLB_ObjectAggregation3D_FindCluster(scene->object_aggregation3d, object);

        BLB_RenderInstances3DGroup *group =
            BLB_ObjectAggregation3D_FindInstanceGroup(scene->object_aggregation3d, object);

        bool dynamic = object_should_use_instance_path(object);

        if (group && group->enabled) {
          if (!dynamic && cluster && !cluster->dirty)
            continue;

          continue;
        }

        if (cluster && !cluster->dirty && !dynamic && !cluster_has_camera_attached_object(cluster))
          continue;
      }

      float local_radius = object_local_radius(object);
      float scale_x = fabsf(object->scale.x);
      float scale_y = fabsf(object->scale.y);
      float scale_z = fabsf(object->scale.z);

      float radius = local_radius * fmaxf(scale_x, fmaxf(scale_y, scale_z));

      if (BLB_OBJECT_SHADOW_CULL_BY_CAMERA && BLB_OBJECT_MAX_SHADOW_RENDER_DISTANCE > 0.0f) {

        BLB_Vec3 delta = BLB_SubV3(object->position, scene->camera->position);

        float max_distance = BLB_OBJECT_MAX_SHADOW_RENDER_DISTANCE + radius;

        if (BLB_DotV3(delta, delta) > max_distance * max_distance)
          continue;
      }

      if (!shadow_sphere_visible(&shadow_frustum, object->position, radius * 1.05f))
        continue;

      BLB_Mat4 model;

      build_model(object->position, object->rotation, object->scale, &model);

      BLB_Mat4 shadow_mvp = BLB_MulM4(shadow_mvp_cache, model);

      BLB_RendererDrawShadowPolygon3D(renderer, active_polygon3d(object), &shadow_mvp.Elements[0][0]);
    }

    BLB_RendererEndShadowPass(renderer);
  }
}

// helpers for physics debug rendering
static void draw_render_objects(BLB_Scene *scene, BLB_Renderer *renderer, BLB_RenderVisibilityCache *visibility_cache,
                                float viewport_width, float viewport_height) {
  BLB_ObjectAggregation3D_ResetDrawState(scene->object_aggregation3d);
  BLB_ObjectOptimization2D_ResetDrawState(scene->object_optimization2d);

  if (scene->camera)
    prepare_camera(scene, renderer, visibility_cache, viewport_width, viewport_height);
  else {
    BLB_Mat4 identity = BLB_M4D(1.0f);

    BLB_RendererSetViewProjection(renderer, &identity.Elements[0][0]);
  }

  if (scene->skybox && scene->camera && scene->camera->camera_cache) {
    draw_skybox(scene->skybox, renderer, scene->camera, scene->camera->camera_cache);
  }

  BLB_CameraCache pixel_cache = {.valid = true,
                                 .view = BLB_M4D(1.0f),
                                 .projection = BLB_M4D(1.0f),
                                 .view_projection = BLB_M4D(1.0f),
                                 .viewport_width = viewport_width,
                                 .viewport_height = viewport_height};
  const BLB_CameraCache *draw_cache = scene->camera ? scene->camera->camera_cache : &pixel_cache;
  BLB_Vec3 camera_position = scene->camera ? scene->camera->position : BLB_V3(0, 0, 0);
  size_t indices[7] = {0, 0, 0, 0, 0, 0, 0};
  size_t counts[7] = {
      (size_t)scene->object3d_count, (size_t)scene->object2d_count, (size_t)scene->text2d_count,
      (size_t)scene->light3d_count,  (size_t)scene->light2d_count,  (size_t)scene->audio2d_count,
      (size_t)scene->audio3d_count,
  };

  for (;;) {
    int best = -1;
    void *best_object = NULL;
    BLB_RenderMode best_mode = BLB_RENDER_OPAQUE;
    int best_layer = 0;

    for (int type = 0; type < 7; ++type) {
      if (indices[type] >= counts[type])
        continue;

      void *object = get_render_object(scene, type, indices[type]);

      if (!object) {
        indices[type]++;
        continue;
      }

      BLB_RenderMode mode = get_render_mode(object, type);
      int layer = get_render_layer(object, type);

      if (!best_object || mode < best_mode || (mode == best_mode && layer < best_layer)) {
        best = type;
        best_object = object;
        best_mode = mode;
        best_layer = layer;
      }
    }

    if (best < 0)
      break;

    switch (best) {
    case 0: {
      BLB_Object3D *object = scene->objects3d[indices[0]];

      if (scene->object_aggregation3d && !object->animation3d && !object->camera_attached && scene->camera &&
          scene->camera->camera_cache) {
        BLB_RenderInstances3DGroup *group =
            BLB_ObjectAggregation3D_FindInstanceGroup(scene->object_aggregation3d, object);
        BLB_RenderObjects3DCluster *cluster = BLB_ObjectAggregation3D_FindCluster(scene->object_aggregation3d, object);

        bool dynamic = object_should_use_instance_path(object);

        if (group && group->enabled && (dynamic || !cluster || cluster->dirty)) {
          if (!group->rendered) {
            draw_instanced_group3d(group, renderer, scene->camera->camera_cache, scene);
            group->rendered = true;
          }

          break;
        }

        if (cluster && !cluster->dirty && !dynamic && cluster->object_count >= 2 &&
            !cluster_has_camera_attached_object(cluster)) {
          if (!cluster->rendered) {
            draw_aggregated_cluster3d(cluster, renderer, scene->camera->camera_cache, scene);

            cluster->rendered = true;
          }
          break;
        }
      }

      if (object && (object->camera_attached || scene->camera) &&
          cached_object3d_visible(visibility_cache, object, draw_cache, camera_position)) {
        draw_object3d(object, renderer, draw_cache, camera_position);
      }
      break;
    }

    case 1: {
      BLB_Object2D *object = scene->objects2d[indices[1]];

      if (scene->object_optimization2d) {
        BLB_RenderInstances2DGroup *group = BLB_ObjectOptimization2D_FindGroup(scene->object_optimization2d, object);

        if (group && instanced_group2d_matches_attached(group, object && object->camera_attached)) {
          if (!group->rendered) {
            draw_instanced_group2d(group, renderer, draw_cache, scene);
            group->rendered = true;
          }
          break;
        }
      }

      if (object && (object->camera_attached || scene->camera) &&
          cached_object2d_visible(visibility_cache, object, draw_cache, camera_position, viewport_width,
                                  viewport_height)) {
        draw_object2d(object, renderer, draw_cache);
      }

      break;
    }

    case 2:
      if (scene->text2d[indices[2]] && !scene->text2d[indices[2]]->camera_attached)
        draw_text2d(scene->text2d[indices[2]], renderer, scene->camera,
                    viewport_height > 0.0f ? viewport_width / viewport_height : 1.0f);
      break;

    case 3:
      if (scene->lights3d[indices[3]] && scene->lights3d[indices[3]]->object && scene->camera &&
          scene->camera->camera_cache &&
          cached_object3d_visible(visibility_cache, scene->lights3d[indices[3]]->object, scene->camera->camera_cache,
                                  scene->camera->position)) {

        draw_object3d(scene->lights3d[indices[3]]->object, renderer, scene->camera->camera_cache,
                      scene->camera->position);

        if (scene->lights3d[indices[3]]->enabled)
          BLB_Object3D_SetTexture(scene->lights3d[indices[3]]->object,
                                  scene->lights3d[indices[3]]->light_debug_texture);
        else
          BLB_Object3D_SetTexture(scene->lights3d[indices[3]]->object,
                                  scene->lights3d[indices[3]]->light_off_debug_texture);
      }
      break;

    case 4: {
      BLB_Light2D *light = scene->lights2d[indices[4]];

      if (!light || !light->object)
        break;

      BLB_Object2D *object = light->object;
      bool visible = scene->camera && scene->camera->camera_cache &&
                     cached_object2d_visible(visibility_cache, object, scene->camera->camera_cache,
                                             scene->camera->position, viewport_width, viewport_height);

      if (visible) {
        draw_object2d(object, renderer, scene->camera->camera_cache);

        if (light->enabled)
          BLB_Object2D_SetTexture(object, light->light_debug_texture);
        else
          BLB_Object2D_SetTexture(object, light->light_off_debug_texture);
      }
      break;
    }

    case 5: {
      BLB_Audio2D *audio = scene->audio2d[indices[5]];

      if (!audio)
        break;

      if (audio->obj && scene->camera && scene->camera->camera_cache &&
          cached_object2d_visible(visibility_cache, audio->obj, scene->camera->camera_cache, scene->camera->position,
                                  viewport_width, viewport_height))
        draw_object2d(audio->obj, renderer, scene->camera->camera_cache);

      if (audio->play) {
        if (audio->surround && scene->camera)
          audio2d_surround_play(audio, scene);
        else
          audio2d_play(audio);
      } else if (audio->obj) {
        BLB_Object2D_SetTexture(audio->obj, audio->audio_off_debug_texture);
      }
      break;
    }

    case 6: {
      if (scene->audio3d[indices[6]] && scene->audio3d[indices[6]]->obj && scene->camera &&
          scene->camera->camera_cache &&
          cached_object3d_visible(visibility_cache, scene->audio3d[indices[6]]->obj, scene->camera->camera_cache,
                                  scene->camera->position)) {

        draw_object3d(scene->audio3d[indices[6]]->obj, renderer, scene->camera->camera_cache, scene->camera->position);
      }

      BLB_Audio3D *audio = scene->audio3d[indices[6]];

      if (audio && audio->obj) {
        if (audio->play) {
          BLB_Object3D_SetTexture(audio->obj, audio->audio_debug_texture);

          if (audio->surround && scene->camera)
            audio3d_surround_play(audio, scene);
          else
            audio3d_play(audio);
        } else {
          BLB_Object3D_SetTexture(audio->obj, audio->audio_off_debug_texture);
        }
      }
      break;
    }
    }

    indices[best]++;
  }

  blb_render_draw_physics_debug_colliders3d(scene, renderer, scene->camera ? scene->camera->camera_cache : NULL);
  blb_render_draw_physics_debug_colliders2d(scene, renderer, scene->camera ? scene->camera->camera_cache : NULL);

  if (BLB_DEBUG)
    blb_render_draw_handler_debug_hitboxes(scene, renderer, scene->camera ? scene->camera->camera_cache : NULL);
}

static bool render_entry_is_camera_attached(const BLB_Scene *scene, int type, size_t index) {
  if (!scene)
    return false;

  return type == 2 && index < (size_t)scene->text2d_count && scene->text2d[index] &&
         scene->text2d[index]->camera_attached;
}

static void draw_camera_attached_objects(BLB_Scene *scene, BLB_Renderer *renderer, float viewport_width,
                                         float viewport_height) {
  if (!scene || !renderer)
    return;

  float aspect = viewport_height > 0.0f ? viewport_width / viewport_height : 1.0f;
  size_t indices[7] = {0, 0, 0, 0, 0, 0, 0};
  size_t counts[7] = {(size_t)scene->object3d_count, (size_t)scene->object2d_count, (size_t)scene->text2d_count,
                      (size_t)scene->light3d_count,  (size_t)scene->light2d_count,  (size_t)scene->audio2d_count,
                      (size_t)scene->audio3d_count};

  for (;;) {
    int best = -1;
    void *best_object = NULL;
    BLB_RenderMode best_mode = BLB_RENDER_OPAQUE;
    int best_layer = 0;

    for (int type = 1; type <= 5; ++type) {
      if (type != 1 && type != 2 && type != 4 && type != 5)
        continue;

      while (indices[type] < counts[type] && !render_entry_is_camera_attached(scene, type, indices[type]))
        indices[type]++;

      if (indices[type] >= counts[type])
        continue;

      void *object = get_render_object(scene, type, indices[type]);

      if (!object) {
        indices[type]++;
        continue;
      }

      BLB_RenderMode mode = get_render_mode(object, type);
      int layer = get_render_layer(object, type);

      if (!best_object || mode < best_mode || (mode == best_mode && layer < best_layer)) {
        best = type;
        best_object = object;
        best_mode = mode;
        best_layer = layer;
      }
    }

    if (best < 0)
      break;

    switch (best) {
    case 1:
    case 4:
    case 5:
      break;

    case 2:
      draw_text2d(scene->text2d[indices[2]], renderer, scene->camera, aspect);
      break;
    }

    indices[best]++;
  }
}

static void draw_particles(BLB_Scene *scene, BLB_Renderer *renderer, float viewport_width, float viewport_height) {
  BLB_CameraCache pixel_cache = {.valid = true,
                                 .view = BLB_M4D(1.0f),
                                 .projection = BLB_M4D(1.0f),
                                 .view_projection = BLB_M4D(1.0f),
                                 .viewport_width = viewport_width,
                                 .viewport_height = viewport_height};
  const BLB_CameraCache *draw_cache = scene->camera ? scene->camera->camera_cache : &pixel_cache;
  BLB_Vec3 camera_position = scene->camera ? scene->camera->position : BLB_V3(0, 0, 0);

  for (int i = 0; i < scene->particle3d_count; ++i) {
    BLB_Particle3D *particle = scene->particles3d[i];

    if (!particle || !particle->visible || (!particle->camera_attached && !scene->camera))
      continue;

    if (BLB_PARTICLE_CULLING && !particle3d_visible_to_camera(particle, draw_cache))
      continue;

    draw_particle3d(particle, renderer, draw_cache, camera_position);
  }

  for (int i = 0; i < scene->particle2d_count; ++i) {
    BLB_Particle2D *particle = scene->particles2d[i];

    if (!particle || !particle->visible)
      continue;

    if (BLB_PARTICLE_CULLING &&
        !particle2d_visible_to_camera(particle, scene->camera ? scene->camera->camera_cache : NULL, viewport_width,
                                      viewport_height))
      continue;

    draw_particle2d(particle, renderer, viewport_width, viewport_height);
  }

  for (int i = 0; i < scene->particle_system3d_count; ++i) {
    BLB_ParticleSystem3D *system = scene->particle_systems3d[i];

    if (system && (scene->camera || system->camera_attached)) {
      draw_particle_system3d(system, renderer, draw_cache, camera_position);
    }
  }

  for (int i = 0; i < scene->particle_system2d_count; ++i) {
    BLB_ParticleSystem2D *system = scene->particle_systems2d[i];

    if (system) {
      draw_particle_system2d(system, renderer, scene->camera ? scene->camera->camera_cache : NULL, viewport_width,
                             viewport_height);
    }
  }
}

static void debug_scene_visibility(BLB_Scene *scene) {
  if (!scene->visibility_cache)
    return;

  if (scene->frame_index > 0 && scene->frame_index % 120u == 0u) {
    BLB_RenderVisibilityCache_Flush(scene->visibility_cache);
    debug_visibility(scene->visibility_cache);
    debug_aggregation(scene);
  }
}

int BLB_DrawScene(BLB_Scene *scene, BLB_Renderer *renderer) {
  if (!scene || !renderer)
    return -1;

  if (!BLB_RendererIsFrameReady(renderer)) {
    int begin_result = BLB_RendererBeginFrame(renderer);

    if (begin_result != 0)
      return begin_result;
  }

  uint32_t viewport_width_u = 0;
  uint32_t viewport_height_u = 0;

  BLB_RendererGetViewport(renderer, &viewport_width_u, &viewport_height_u);
  float viewport_width = (float)viewport_width_u;
  float viewport_height = (float)viewport_height_u;
  int result = 0;

  if (scene->delta_time_revision == BLB_DeltaTime_GetRevision())
    BLB_DeltaTime_Update();

  if (!scene->delta_time_manual)
    BLB_SetSceneDeltaTime(scene, BLB_DeltaTime_Get());

  scene->delta_time_manual = false;
  scene->delta_time_revision = BLB_DeltaTime_GetRevision();

  BLB_ObjectLOD_BeginFrame(scene->frame_index);
  prepare_clear(scene, renderer);

  if (scene->camera && scene->camera->delta_time) {
    *scene->camera->delta_time = scene->delta_time;
  }

  update_scene(scene);

  prepare_sort(scene);
  prepare_optimization(scene, renderer);
  prepare_renderer_lighting(scene, renderer);

  BLB_Mat4 shadow_vp = BLB_M4D(1.0f);

  BLB_Mat4 point_shadow_mvp[BLB_RENDER_POINT_SHADOW_FACES];
  BLB_Light3D *point_shadow_light = NULL;
  uint32_t shadow_mode = prepare_shadows(scene, renderer, &shadow_vp, point_shadow_mvp, &point_shadow_light);
  result = prepare_instance_capacity(scene, renderer, shadow_mode);

  if (result != 0)
    return result;

  draw_shadows(scene, renderer, shadow_mode);
  BLB_RendererBeginMainPass(renderer);

  if (scene->enabled && scene->visible) {
    BLB_RenderVisibilityCache *visibility_cache = scene->visibility_cache;
    draw_render_objects(scene, renderer, visibility_cache, viewport_width, viewport_height);
    draw_particles(scene, renderer, viewport_width, viewport_height);
    draw_camera_attached_objects(scene, renderer, viewport_width, viewport_height);
    debug_scene_visibility(scene);
  }

  return BLB_RendererEndFrame(renderer);
}
