#include "bulba/core/render.h"

#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/math3v/lights.h"
#include "bulba/core/objects3d/objects3d.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef BLB_GLOW_STEPS_3D
#define BLB_GLOW_STEPS_3D 8
#endif

#ifndef BLB_GLOW_STEPS_2D
#define BLB_GLOW_STEPS_2D 8
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

static void rgb(const unsigned char color[4], float output[4]) {
  output[0] = color[0] / 255.0f;
  output[1] = color[1] / 255.0f;
  output[2] = color[2] / 255.0f;
  output[3] = color[3] / 255.0f;
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

  if (object->material)
    return normalize_render_mode(object->material->render_mode);

  return normalize_render_mode(object->render_mode);
}

static BLB_RenderMode object2d_render_mode(const BLB_Object2D *object) {

  if (!object)
    return BLB_RENDER_OPAQUE;

  if (object->material)
    return normalize_render_mode(object->material->render_mode);

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
    if (index < (size_t)scene->object3d_count)
      return scene->objects3d[index];
    return NULL;

  case 1:
    if (index < (size_t)scene->object2d_count)
      return scene->objects2d[index];
    return NULL;

  case 2:
    if (index < (size_t)scene->text2d_count)
      return scene->text2d[index];
    return NULL;

  case 3:
    if (index < (size_t)scene->light3d_count && scene->lights3d[index])
      return scene->lights3d[index]->object;
    return NULL;

  case 4:
    if (index < (size_t)scene->light2d_count && scene->lights2d[index])
      return scene->lights2d[index]->object;
    return NULL;
  }

  return NULL;
}

static BLB_RenderMode get_render_mode(void *object, int type) {

  if (!object)
    return BLB_RENDER_OPAQUE;

  if (type == 0 || type == 3)
    return object3d_render_mode((BLB_Object3D *)object);

  if (type == 1 || type == 4)
    return object2d_render_mode((BLB_Object2D *)object);

  return text2d_render_mode((BLB_Text2D *)object);
}

static int get_render_layer(void *object, int type) {

  if (!object)
    return 0;

  if (type == 0 || type == 3)
    return ((BLB_Object3D *)object)->layer;

  if (type == 1 || type == 4)
    return ((BLB_Object2D *)object)->layer;

  return ((BLB_Text2D *)object)->layer;
}

static BLB_RenderMaterialState material_state_from_3d(const BLB_Object3D *object) {

  BLB_RenderMaterialState state = {0};

  if (!object)
    return state;

  if (object->material) {
    const BLB_Material *material = object->material;

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

  state.emission = object->emission;

  state.glow = object->glow;

  state.glow_radius = object->glow > 0.0f ? 1.0f : 0.0f;

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

  state.emission = object->emission;

  state.glow = object->glow;

  state.glow_radius = object->glow > 0.0f ? 18.0f : 0.0f;

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

static void build_model(HMM_Vec3 position, HMM_Vec3 rotation, HMM_Vec3 scale, HMM_Mat4 *model) {
  HMM_Mat4 rx = HMM_Rotate_RH(HMM_AngleDeg(rotation.x), HMM_V3(1.0f, 0.0f, 0.0f));
  HMM_Mat4 ry = HMM_Rotate_RH(HMM_AngleDeg(rotation.y), HMM_V3(0.0f, 1.0f, 0.0f));
  HMM_Mat4 rz = HMM_Rotate_RH(HMM_AngleDeg(rotation.z), HMM_V3(0.0f, 0.0f, 1.0f));
  HMM_Mat4 rotation_matrix = HMM_MulM4(rz, HMM_MulM4(ry, rx));
  *model = HMM_MulM4(HMM_Translate(position), HMM_MulM4(rotation_matrix, HMM_Scale(scale)));
}

static void extract_matrix_rows(const HMM_Mat4 *matrix, float rows[12]) {

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

static bool build_shadow_matrix(BLB_Scene *scene, HMM_Mat4 *shadow_vp) {

  if (!scene || !shadow_vp)
    return false;

  BLB_Light3D *light = NULL;

  for (int i = 0; i < scene->light3d_count; i++) {

    BLB_Light3D *candidate = scene->lights3d[i];

    if (!candidate || !candidate->enabled || candidate->type != BLB_LIGHT_DIRECTIONAL)
      continue;

    light = candidate;
    break;
  }

  if (!light)
    return false;

  HMM_Vec3 direction = BLB_GetLightDirection3D(light);

  float target_z = -20.0f;

  if (scene->object3d_count > 0) {
    HMM_Vec3 average = HMM_V3(0.0f, 0.0f, 0.0f);

    int count = 0;

    for (int i = 0; i < scene->object3d_count; i++) {

      BLB_Object3D *object = scene->objects3d[i];

      if (!object || !object->visible)
        continue;

      average = HMM_AddV3(average, object->position);

      count++;
    }

    if (count > 0)
      target_z = HMM_MulV3F(average, 1.0f / (float)count).z;
  }

  HMM_Vec3 target = HMM_V3(0.0f, 0.0f, target_z);

  HMM_Vec3 eye = HMM_SubV3(target, HMM_MulV3F(direction, 60.0f));

  HMM_Vec3 up = fabsf(HMM_DotV3(direction, HMM_V3(0.0f, 1.0f, 0.0f))) > 0.98f ? HMM_V3(0.0f, 0.0f, 1.0f) : HMM_V3(0.0f, 1.0f, 0.0f);

  HMM_Mat4 view = HMM_LookAt_RH(eye, target, up);

  HMM_Mat4 projection = HMM_Orthographic_RH_ZO(-35.0f, 35.0f, 35.0f, -35.0f, 0.1f, 140.0f);

  *shadow_vp = HMM_MulM4(projection, view);

  return true;
}

static bool build_point_shadow_matrices(BLB_Scene *scene, HMM_Mat4 shadow_mvp[VULKAN_POINT_SHADOW_FACES], BLB_Light3D **shadow_light) {
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

  HMM_Vec3 position = light->object->position;
  float far_plane = fmaxf(light->range, 10.0f);
  HMM_Mat4 projection = HMM_Perspective_RH_ZO(HMM_AngleDeg(90.0f), 1.0f, 0.05f, far_plane);
  HMM_Vec3 directions[VULKAN_POINT_SHADOW_FACES] = {
      HMM_V3(1.0f, 0.0f, 0.0f),  HMM_V3(-1.0f, 0.0f, 0.0f), HMM_V3(0.0f, 1.0f, 0.0f),
      HMM_V3(0.0f, -1.0f, 0.0f), HMM_V3(0.0f, 0.0f, 1.0f),  HMM_V3(0.0f, 0.0f, -1.0f),
  };
  HMM_Vec3 ups[VULKAN_POINT_SHADOW_FACES] = {
      HMM_V3(0.0f, -1.0f, 0.0f), HMM_V3(0.0f, -1.0f, 0.0f), HMM_V3(0.0f, 0.0f, 1.0f),
      HMM_V3(0.0f, 0.0f, -1.0f), HMM_V3(0.0f, -1.0f, 0.0f), HMM_V3(0.0f, -1.0f, 0.0f),
  };

  for (uint32_t i = 0; i < VULKAN_POINT_SHADOW_FACES; i++) {
    HMM_Mat4 view = HMM_LookAt_RH(position, HMM_AddV3(position, directions[i]), ups[i]);
    shadow_mvp[i] = HMM_MulM4(projection, view);
  }

  *shadow_light = light;
  return true;
}

static VulkanMaterial vulkan_material_from_state(const BLB_RenderMaterialState *state, bool lighting) {

  VulkanMaterial result = {0};

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

static void draw_object3d_pass(BLB_Object3D *object, VULKAN *renderer, const BLB_CameraCache *camera_cache, const BLB_RenderMaterialState *state,
                               float scale_mul, float glow_mul) {

  if (!object || !renderer || !camera_cache || !camera_cache->valid || !state)
    return;

  HMM_Vec3 pass_scale = HMM_MulV3F(object->scale, scale_mul);

  HMM_Mat4 model;
  build_model(object->position, object->rotation, pass_scale, &model);

  HMM_Mat4 mvp = HMM_MulM4(camera_cache->projection, HMM_MulM4(camera_cache->view, model));

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

  VulkanMaterial vk_material = vulkan_material_from_state(&pass, !glow_pass);
  vk_material.base_texture_override = object->texture;

  VULKAN_RendererDrawPolygon3D(renderer, object->polygon, &mvp.Elements[0][0], model_rows, pass.base_color[0], pass.base_color[1], pass.base_color[2],
                               pass.base_color[3], &vk_material, object->texture);
}

static void draw_object3d(BLB_Object3D *object, VULKAN *renderer, const BLB_CameraCache *camera_cache) {

  if (!object || !renderer || !camera_cache || !camera_cache->valid || !object->visible || !object->polygon)
    return;

  BLB_RenderMaterialState state = material_state_from_3d(object);

  if (state.glow > 0.0f && state.glow_radius > 0.0f) {
    const int steps = BLB_GLOW_STEPS_3D > 0 ? BLB_GLOW_STEPS_3D : 1;
    const float radius = state.glow_radius;
    const float falloff = fmaxf(state.glow_falloff, 0.2f);

    /*
     * Render the additive shell before the solid object. The glow material
     * does not write depth, so the object's own depth buffer entry cannot
     * hide the inner side of a closed mesh such as a torus. The base pass is
     * then rendered normally and writes the final opaque depth on top.
     */
    for (int i = 1; i <= steps; i++) {
      float t = (float)i / (float)steps;
      float envelope = powf(fmaxf(0.0f, 1.0f - t), falloff);
      float smooth_envelope = envelope * (0.92f + 0.08f * (1.0f - t));
      float scale_mul = 1.0f + radius * 0.019f * t;
      float glow_mul = smooth_envelope * state.glow * 0.22f;

      draw_object3d_pass(object, renderer, camera_cache, &state, scale_mul, glow_mul);
    }
  }

  draw_object3d_pass(object, renderer, camera_cache, &state, 1.0f, 1.0f);
}

static void draw_object2d_pass(BLB_Object2D *object, VULKAN *renderer, const BLB_CameraCache *camera_cache, const BLB_RenderMaterialState *state,
                               float scale_mul, float glow_mul) {
  if (!object || !renderer || !object->polygon || !state)
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

  VulkanMaterial vk_material = vulkan_material_from_state(&pass, !glow_pass);

  const size_t count = object->polygon->vertex_count;
  if (count == 0)
    return;

  const float viewport_width = (float)renderer->swapchain_extent.width;
  const float viewport_height = (float)renderer->swapchain_extent.height;

  HMM_Vec2 vertices[count];
  HMM_Vec2 world_positions[count];
  HMM_Vec2 uvs[count];

  const bool has_uvs = object->polygon->uvs != NULL;
  const bool flip_uv_y = !object->screen_space && camera_cache && camera_cache->valid;

  const float angle = HMM_AngleDeg(object->rotation);
  const float c = cosf(angle);
  const float s = sinf(angle);

  const HMM_Mat4 *vp = NULL;

  if (!object->screen_space && camera_cache && camera_cache->valid)
    vp = &camera_cache->view_projection;

  BLB_Texture *texture = object->texture;

  if (object->animation && object->animation->textures && object->animation->textures_count > 0) {
    size_t frame = object->animation->texture_counter;

    if (frame >= object->animation->textures_count)
      frame = 0;

    texture = object->animation->textures[frame];
  }

  for (size_t i = 0; i < count; i++) {
    const float x = object->polygon->vertices[i].x * object->scale.x * scale_mul;

    const float y = object->polygon->vertices[i].y * object->scale.y * scale_mul;

    const float transformed_x = x * c - y * s;

    const float transformed_y = x * s + y * c;

    const float world_x = object->position.x + transformed_x;

    const float world_y = object->position.y + transformed_y;

    world_positions[i] = HMM_V2(world_x, world_y);

    if (has_uvs) {
      uvs[i] = object->polygon->uvs[i];

      if (flip_uv_y)
        uvs[i].y = 1.0f - uvs[i].y;
    }

    if (object->screen_space || !vp) {
      vertices[i] = HMM_V2(world_x, world_y);
      continue;
    }

    HMM_Vec4 world_position = HMM_V4(world_x, world_y, 0.0f, 1.0f);

    HMM_Vec4 clip = HMM_MulM4V4(*vp, world_position);

    if (fabsf(clip.w) <= 0.000001f) {
      vertices[i] = HMM_V2(-100000.0f, -100000.0f);
      continue;
    }

    const float inv_w = 1.0f / clip.w;
    const float ndc_x = clip.x * inv_w;
    const float ndc_y = clip.y * inv_w;

    vertices[i].x = (ndc_x * 0.5f + 0.5f) * viewport_width;

    vertices[i].y = (1.0f - (ndc_y * 0.5f + 0.5f)) * viewport_height;
  }

  BLB_Polygon2D transformed = *object->polygon;
  transformed.vertices = vertices;

  if (has_uvs)
    transformed.uvs = uvs;

  VULKAN_RendererDrawPolygon2D(renderer, &transformed, world_positions, viewport_width, viewport_height, pass.base_color[0], pass.base_color[1],
                               pass.base_color[2], pass.base_color[3], &vk_material, texture);
}

static void draw_object2d(BLB_Object2D *object, VULKAN *renderer, const BLB_CameraCache *camera_cache) {
  if (!object || !renderer || !object->visible || !object->polygon)
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

static void draw_text2d(BLB_Text2D *text, VULKAN *renderer, BLB_Camera *camera, float aspect) {

  if (!text || !renderer || !text->visible || !text->text || !text->font_path || !text->font_loaded)
    return;

  (void)camera;
  (void)aspect;

  if (VULKAN_RendererLoadFont(renderer, &text->font) != 0)
    return;

  BLB_RenderMaterialState state = material_state_from_text(text);

  float glyph_scale = text->font.size > 0 ? text->size / (float)text->font.size : 1.0f;

  if (glyph_scale <= 0.0f)
    glyph_scale = 0.001f;

  VULKAN_RendererDrawText(renderer, &text->font, text->text, text->position.x, text->position.y, glyph_scale, text->scale, 0.0f, state.base_color[0],
                          state.base_color[1], state.base_color[2], state.base_color[3], state.emission, state.glow, state.roughness,
                          normalize_render_mode(state.render_mode));
}

static uint64_t render_sort_signature3d(BLB_Object3D **objects, int count) {
  uint64_t h = UINT64_C(1469598103934665603);
  for (int i = 0; i < count; ++i) {
    uintptr_t p = (uintptr_t)objects[i];
    BLB_Object3D *o = objects[i];
    uint64_t v = (uint64_t)p ^ ((uint64_t)(o ? (uint32_t)(o->layer + 32768) : 0u) << 32u) ^ (uint64_t)(o ? (uint32_t)object3d_render_mode(o) : 0u);
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
    uint64_t v = (uint64_t)p ^ ((uint64_t)(o ? (uint32_t)(o->layer + 32768) : 0u) << 32u) ^ (uint64_t)(o ? (uint32_t)object2d_render_mode(o) : 0u);
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
    uint64_t v = (uint64_t)p ^ ((uint64_t)(o ? (uint32_t)(o->layer + 32768) : 0u) << 32u) ^ (uint64_t)(o ? (uint32_t)o->render_mode : 0u);
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

int BLB_DrawScene(BLB_Scene *scene, VULKAN *renderer) {
  if (!scene || !renderer)
    return -1;

  int result = VULKAN_RendererBeginFrame(renderer);

  if (result != 0)
    return result;

  if (scene->clear_enabled) {
    VULKAN_RendererSetClearColor(renderer, scene->clear_color[0] / 255.0f, scene->clear_color[1] / 255.0f, scene->clear_color[2] / 255.0f,
                                 scene->clear_color[3] / 255.0f);
  } else {
    VULKAN_RendererSetClearColor(renderer, 0.0f, 0.0f, 0.0f, 1.0f);
  }

  if (scene->camera && scene->camera->delta_time)
    *scene->camera->delta_time = scene->delta_time;

  int max_count = scene->main_count;

  if (scene->object3d_count > max_count)
    max_count = scene->object3d_count;

  if (scene->object2d_count > max_count)
    max_count = scene->object2d_count;

  if (scene->text2d_count > max_count)
    max_count = scene->text2d_count;

  if (scene->light3d_count > max_count)
    max_count = scene->light3d_count;

  if (scene->light2d_count > max_count)
    max_count = scene->light2d_count;

  if (scene->enabled) {
    for (int i = 0; i < max_count; i++) {

      if (i < scene->object3d_count) {

        BLB_Object3D *object = scene->objects3d[i];

        if (object && object->delta_time)
          *object->delta_time = scene->delta_time;
      }

      if (i < scene->object2d_count) {

        BLB_Object2D *object = scene->objects2d[i];

        if (object && object->delta_time)
          *object->delta_time = scene->delta_time;

        if (object && object->animation && object->animation->enable && object->animation->textures && object->animation->textures_count > 0) {

          object->animation->frame_count += scene->delta_time;

          while (object->animation->frame_count >= object->animation->frame_time) {
            object->animation->frame_count -= object->animation->frame_time;
            object->animation->texture_counter = (object->animation->texture_counter + 1) % object->animation->textures_count;
            BLB_Object2D_SetTexture(object, object->animation->textures[object->animation->texture_counter]);
          }
        }
      }

      if (i < scene->text2d_count) {

        BLB_Text2D *text = scene->text2d[i];

        if (text && text->delta_time)
          *text->delta_time = scene->delta_time;
      }

      if (i < scene->light3d_count) {

        BLB_Light3D *light = scene->lights3d[i];

        if (light && light->object && light->object->delta_time)
          *light->object->delta_time = scene->delta_time;
      }

      if (i < scene->light2d_count) {

        BLB_Light2D *light = scene->lights2d[i];

        if (light && light->object && light->object->delta_time)
          *light->object->delta_time = scene->delta_time;
      }
    }
  }

  if (scene->camera)
    VULKAN_RendererSetCameraPosition(renderer, scene->camera->position);
  else
    VULKAN_RendererSetCameraPosition(renderer, HMM_V3(0.0f, 0.0f, 0.0f));

  VULKAN_RendererSetLights3D(renderer, scene->lights3d, scene->light3d_count);

  VULKAN_RendererSetLights2D(renderer, scene->lights2d, scene->light2d_count);

  VULKAN_RendererSetLightingObjects3D(renderer, scene->objects3d, scene->object3d_count);

  VULKAN_RendererSetLightingObjects2D(renderer, scene->objects2d, scene->object2d_count);

  HMM_Mat4 shadow_vp = HMM_M4D(1.0f);
  HMM_Mat4 point_shadow_mvp[VULKAN_POINT_SHADOW_FACES];
  BLB_Light3D *point_shadow_light = NULL;

  bool scene_draw_enabled = scene->enabled && scene->visible;

  bool directional_shadow = scene_draw_enabled && build_shadow_matrix(scene, &shadow_vp);
  bool point_shadow = scene_draw_enabled && !directional_shadow && build_point_shadow_matrices(scene, point_shadow_mvp, &point_shadow_light);

  if (directional_shadow) {
    VULKAN_RendererSetShadow(renderer, &shadow_vp.Elements[0][0], true, 0.003f);
  } else if (point_shadow) {
    VULKAN_RendererSetPointShadow(renderer, point_shadow_mvp, true, 0.002f);
    renderer->shadow_light3d = point_shadow_light;
  } else {
    VULKAN_RendererSetShadow(renderer, &shadow_vp.Elements[0][0], false, 0.003f);
  }

  update_light_buffer_3d(renderer);
  update_light_buffer_2d(renderer);

  if (renderer->shadow_mode != 0) {
    uint32_t shadow_count = renderer->shadow_mode == 2 ? VULKAN_POINT_SHADOW_FACES : 1;

    uint32_t first_shadow_map = renderer->shadow_mode == 2 ? 1u : 0u;

    for (uint32_t pass = 0; pass < shadow_count; pass++) {

      uint32_t shadow_map_index = first_shadow_map + pass;

      VULKAN_RendererBeginShadowPass(renderer, shadow_map_index);

      for (int i = 0; i < scene->object3d_count; i++) {

        BLB_Object3D *object = scene->objects3d[i];

        if (!object || !object->visible || !object->polygon)
          continue;

        if (object3d_render_mode(object) != BLB_RENDER_OPAQUE)
          continue;

        HMM_Mat4 model;
        build_model(object->position, object->rotation, object->scale, &model);

        HMM_Mat4 shadow_mvp = HMM_MulM4(renderer->shadow_mvp[shadow_map_index], model);

        VULKAN_RendererDrawShadowPolygon3D(renderer, object->polygon, &shadow_mvp.Elements[0][0]);
      }

      VULKAN_RendererEndShadowPass(renderer);
    }
  }

  VULKAN_RendererBeginMainPass(renderer);

  if (scene_draw_enabled) {
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

    float aspect = renderer->swapchain_extent.height ? (float)renderer->swapchain_extent.width / (float)renderer->swapchain_extent.height : 1.0f;

    if (scene->camera) {
      scene->camera->camera_cache->valid = true;
      scene->camera->camera_cache->view = BLB_CameraView(scene->camera);
      scene->camera->camera_cache->projection = BLB_CameraProjection(scene->camera, aspect);
      scene->camera->camera_cache->view_projection = HMM_MulM4(scene->camera->camera_cache->projection, scene->camera->camera_cache->view);
    }

    size_t indices[5] = {
        0, 0, 0, 0, 0,
    };

    size_t counts[5] = {
        (size_t)scene->object3d_count, (size_t)scene->object2d_count, (size_t)scene->text2d_count,
        (size_t)scene->light3d_count,  (size_t)scene->light2d_count,
    };

    for (;;) {
      int best = -1;
      void *best_object = NULL;
      BLB_RenderMode best_mode = BLB_RENDER_OPAQUE;
      int best_layer = 0;

      for (int type = 0; type < 5; type++) {

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
      case 0:
        draw_object3d(scene->objects3d[indices[0]], renderer, scene->camera->camera_cache);
        break;

      case 1:
        draw_object2d(scene->objects2d[indices[1]], renderer, scene->camera->camera_cache);
        break;

      case 2:
        draw_text2d(scene->text2d[indices[2]], renderer, scene->camera, aspect);
        break;

      case 3:
        if (scene->lights3d[indices[3]] && scene->lights3d[indices[3]]->object) {

          draw_object3d(scene->lights3d[indices[3]]->object, renderer, scene->camera->camera_cache);
        }
        break;

      case 4:
        if (scene->lights2d[indices[4]] && scene->lights2d[indices[4]]->object) {

          draw_object2d(scene->lights2d[indices[4]]->object, renderer, scene->camera->camera_cache);
        }
        break;
      }

      indices[best]++;
    }
  }

  return VULKAN_RendererEndFrame(renderer);
}
