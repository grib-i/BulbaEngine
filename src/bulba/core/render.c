#include "bulba/core/render.h"

#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/math3v/lights.h"
#include "bulba/core/objects3d/objects3d.h"

#include <math.h>
#include <stdlib.h>

typedef struct {
  float base_color[4];
  float emission;
  float glow;
  float glow_radius;
  float glow_falloff;
  float roughness;
  float alpha_cutoff;
  bool lighting_enabled;
  bool depth_enabled;
  bool depth_write;
  bool double_sided;
  bool unlit;
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

    state.alpha_cutoff = material->alpha_cutoff;

    state.lighting_enabled = material->lighting_enabled;

    state.depth_enabled = material->depth_enabled;

    state.depth_write = material->depth_write;

    state.double_sided = material->double_sided;

    state.unlit = material->unlit;

    state.render_mode = normalize_render_mode(material->render_mode);

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

  state.alpha_cutoff = 0.5f;

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

    state.alpha_cutoff = material->alpha_cutoff;

    state.lighting_enabled = material->lighting_enabled;

    state.depth_enabled = material->depth_enabled;

    state.depth_write = material->depth_write;

    state.double_sided = material->double_sided;

    state.unlit = material->unlit;

    state.render_mode = normalize_render_mode(material->render_mode);

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

  state.alpha_cutoff = 0.5f;

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

    state.alpha_cutoff = material->alpha_cutoff;

    state.lighting_enabled = false;
    state.depth_enabled = material->depth_enabled;

    state.depth_write = material->depth_write;

    state.double_sided = material->double_sided;

    state.unlit = true;

    state.render_mode = normalize_render_mode(material->render_mode);

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

static void build_model(HMM_Vec3 position, HMM_Vec3 rotation, HMM_Vec3 scale, HMM_Mat4 *model, HMM_Mat4 *rotation_matrix, float normal_rows[12]) {

  HMM_Mat4 rx = HMM_Rotate_RH(HMM_AngleDeg(rotation.x), HMM_V3(1.0f, 0.0f, 0.0f));

  HMM_Mat4 ry = HMM_Rotate_RH(HMM_AngleDeg(rotation.y), HMM_V3(0.0f, 1.0f, 0.0f));

  HMM_Mat4 rz = HMM_Rotate_RH(HMM_AngleDeg(rotation.z), HMM_V3(0.0f, 0.0f, 1.0f));

  *rotation_matrix = HMM_MulM4(rz, HMM_MulM4(ry, rx));

  *model = HMM_MulM4(HMM_Translate(position), HMM_MulM4(*rotation_matrix, HMM_Scale(scale)));

  float sx = fabsf(scale.x) > 0.000001f ? scale.x : 1.0f;

  float sy = fabsf(scale.y) > 0.000001f ? scale.y : 1.0f;

  float sz = fabsf(scale.z) > 0.000001f ? scale.z : 1.0f;

  normal_rows[0] = rotation_matrix->Elements[0][0] / sx;

  normal_rows[1] = rotation_matrix->Elements[1][0] / sy;

  normal_rows[2] = rotation_matrix->Elements[2][0] / sz;

  normal_rows[3] = 0.0f;

  normal_rows[4] = rotation_matrix->Elements[0][1] / sx;

  normal_rows[5] = rotation_matrix->Elements[1][1] / sy;

  normal_rows[6] = rotation_matrix->Elements[2][1] / sz;

  normal_rows[7] = 0.0f;

  normal_rows[8] = rotation_matrix->Elements[0][2] / sx;

  normal_rows[9] = rotation_matrix->Elements[1][2] / sy;

  normal_rows[10] = rotation_matrix->Elements[2][2] / sz;

  normal_rows[11] = 0.0f;
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

static VulkanMaterial vulkan_material_from_state(const BLB_RenderMaterialState *state, bool lighting) {

  VulkanMaterial result = {0};

  if (!state)
    return result;

  result.lighting_enabled = lighting && state->lighting_enabled;

  result.emission = state->emission;

  result.glow = state->glow;

  result.roundness = state->roughness;

  result.glow_radius = state->glow_radius;

  result.glow_falloff = state->glow_falloff;

  result.render_mode = normalize_render_mode(state->render_mode);

  return result;
}

static void draw_object3d_pass(BLB_Object3D *object, VULKAN *renderer, BLB_Camera *camera, float aspect, const BLB_RenderMaterialState *state,
                               float scale_mul, float glow_mul) {

  if (!object || !renderer || !camera || !state)
    return;

  HMM_Vec3 pass_scale = HMM_MulV3F(object->scale, scale_mul);

  HMM_Mat4 model;
  HMM_Mat4 rotation;

  float normal_rows[12];

  build_model(object->position, object->rotation, pass_scale, &model, &rotation, normal_rows);

  HMM_Mat4 view = BLB_CameraView(camera);

  HMM_Mat4 projection = BLB_CameraProjection(camera, aspect);

  HMM_Mat4 mvp = HMM_MulM4(projection, HMM_MulM4(view, model));

  float model_rows[12];

  extract_matrix_rows(&model, model_rows);

  BLB_RenderMaterialState pass = *state;

  pass.emission *= glow_mul;
  pass.glow *= glow_mul;
  pass.base_color[3] *= glow_mul;

  VulkanMaterial vk_material = vulkan_material_from_state(&pass, true);

  VULKAN_RendererDrawPolygon3D(renderer, object->polygon, &mvp.Elements[0][0], model_rows, normal_rows, pass.base_color[0], pass.base_color[1],
                               pass.base_color[2], pass.base_color[3], &vk_material, object->texture);
}

static void draw_object3d(BLB_Object3D *object, VULKAN *renderer, BLB_Camera *camera, float aspect) {

  if (!object || !renderer || !camera || !object->visible || !object->polygon)
    return;

  BLB_RenderMaterialState state = material_state_from_3d(object);

  draw_object3d_pass(object, renderer, camera, aspect, &state, 1.0f, 1.0f);

  if (state.glow <= 0.0f || state.glow_radius <= 0.0f)
    return;

  const int steps = 6;
  const float radius = state.glow_radius;

  const float falloff = fmaxf(state.glow_falloff, 0.2f);

  for (int i = 1; i <= steps; i++) {

    float t = (float)i / (float)steps;

    float envelope = powf(fmaxf(0.0f, 1.0f - t), falloff);

    float scale_mul = 1.0f + radius * 0.035f * t;

    float glow_mul = envelope * state.glow * 0.32f;

    draw_object3d_pass(object, renderer, camera, aspect, &state, scale_mul, glow_mul);
  }
}

static void draw_object2d_pass(BLB_Object2D *object, VULKAN *renderer, BLB_Camera *camera, float aspect, const BLB_RenderMaterialState *state,
                               float scale_mul, float glow_mul) {

  if (!object || !renderer || !object->polygon || !state)
    return;

  BLB_RenderMaterialState pass = *state;

  pass.emission *= glow_mul;
  pass.glow *= glow_mul;
  pass.base_color[3] *= glow_mul;

  VulkanMaterial vk_material = vulkan_material_from_state(&pass, false);

  float viewport_width = (float)renderer->swapchain_extent.width;

  float viewport_height = (float)renderer->swapchain_extent.height;

  size_t count = object->polygon->vertex_count;

  if (count == 0)
    return;

  HMM_Vec2 vertices[count];

  float angle = HMM_AngleDeg(object->rotation);

  float c = cosf(angle);
  float s = sinf(angle);

  HMM_Mat4 vp = HMM_M4D(1.0f);

  if (!object->screen_space && camera) {

    HMM_Mat4 view = BLB_CameraView(camera);

    HMM_Mat4 projection = BLB_CameraProjection(camera, aspect);

    vp = HMM_MulM4(projection, view);
  }

  for (size_t i = 0; i < count; i++) {

    float x = object->polygon->vertices[i].x * object->scale.x * scale_mul;

    float y = object->polygon->vertices[i].y * object->scale.y * scale_mul;

    float world_x = object->position.x + x * c - y * s;

    float world_y = object->position.y + x * s + y * c;

    if (object->screen_space || !camera) {

      vertices[i] = HMM_V2(world_x, world_y);

      continue;
    }

    HMM_Vec4 position = HMM_V4(world_x, world_y, 0.0f, 1.0f);

    HMM_Vec4 clip = HMM_MulM4V4(vp, position);

    if (fabsf(clip.w) <= 0.000001f) {

      vertices[i] = HMM_V2(-100000.0f, -100000.0f);

      continue;
    }

    float inv_w = 1.0f / clip.w;

    float ndc_x = clip.x * inv_w;

    float ndc_y = clip.y * inv_w;

    vertices[i].x = (ndc_x * 0.5f + 0.5f) * viewport_width;

    vertices[i].y = (1.0f - (ndc_y * 0.5f + 0.5f)) * viewport_height;
  }

  BLB_Polygon2D transformed = *object->polygon;

  transformed.vertices = vertices;

  VULKAN_RendererDrawPolygon2D(renderer, &transformed, viewport_width, viewport_height, pass.base_color[0], pass.base_color[1], pass.base_color[2],
                               pass.base_color[3], &vk_material, object->texture);
}

static void draw_object2d(BLB_Object2D *object, VULKAN *renderer, BLB_Camera *camera, float aspect) {

  if (!object || !renderer || !object->visible || !object->polygon)
    return;

  BLB_RenderMaterialState state = material_state_from_2d(object);

  draw_object2d_pass(object, renderer, camera, aspect, &state, 1.0f, 1.0f);

  if (state.glow <= 0.0f || state.glow_radius <= 0.0f)
    return;

  float base_size = fmaxf(fabsf(object->scale.x), fmaxf(fabsf(object->scale.y), 1.0f));

  const int steps = 7;

  const float falloff = fmaxf(state.glow_falloff, 0.2f);

  for (int i = 1; i <= steps; i++) {

    float t = (float)i / (float)steps;

    float envelope = powf(fmaxf(0.0f, 1.0f - t), falloff);

    float scale_mul = 1.0f + (state.glow_radius / base_size) * t;

    float glow_mul = envelope * state.glow * 0.22f;

    draw_object2d_pass(object, renderer, camera, aspect, &state, scale_mul, glow_mul);
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

  if (scene->camera) {
    VULKAN_RendererSetCameraPosition(renderer, scene->camera->position);
  } else {
    VULKAN_RendererSetCameraPosition(renderer, HMM_V3(0.0f, 0.0f, 0.0f));
  }

  VULKAN_RendererSetLights3D(renderer, scene->lights3d, scene->light3d_count);

  VULKAN_RendererSetLights2D(renderer, scene->lights2d, scene->light2d_count);

  HMM_Mat4 shadow_vp = HMM_M4D(1.0f);

  bool scene_draw_enabled = scene->enabled && scene->visible;

  bool shadow_enabled = scene_draw_enabled && build_shadow_matrix(scene, &shadow_vp);

  VULKAN_RendererSetShadow(renderer, &shadow_vp.Elements[0][0], shadow_enabled, 0.002f);

  if (shadow_enabled) {
    VULKAN_RendererBeginShadowPass(renderer);

    for (int i = 0; i < scene->object3d_count; i++) {

      BLB_Object3D *object = scene->objects3d[i];

      if (!object || !object->visible || !object->polygon)
        continue;

      if (object3d_render_mode(object) != BLB_RENDER_OPAQUE)
        continue;

      HMM_Mat4 model;
      HMM_Mat4 rotation;

      float normal_rows[12];

      build_model(object->position, object->rotation, object->scale, &model, &rotation, normal_rows);

      HMM_Mat4 shadow_mvp = HMM_MulM4(shadow_vp, model);

      VULKAN_RendererDrawShadowPolygon3D(renderer, object->polygon, &shadow_mvp.Elements[0][0]);
    }

    VULKAN_RendererEndShadowPass(renderer);
  }

  VULKAN_RendererBeginMainPass(renderer);

  if (scene_draw_enabled) {
    if (scene->object3d_count > 1)
      qsort(scene->objects3d, scene->object3d_count, sizeof(BLB_Object3D *), compare_object3d);

    if (scene->object2d_count > 1)
      qsort(scene->objects2d, scene->object2d_count, sizeof(BLB_Object2D *), compare_object2d);

    if (scene->text2d_count > 1)
      qsort(scene->text2d, scene->text2d_count, sizeof(BLB_Text2D *), compare_text2d);

    if (scene->light3d_count > 1)
      qsort(scene->lights3d, scene->light3d_count, sizeof(BLB_Light3D *), compare_light3d);

    if (scene->light2d_count > 1)
      qsort(scene->lights2d, scene->light2d_count, sizeof(BLB_Light2D *), compare_light2d);

    float aspect = renderer->swapchain_extent.height ? (float)renderer->swapchain_extent.width / (float)renderer->swapchain_extent.height : 1.0f;

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
        draw_object3d(scene->objects3d[indices[0]], renderer, scene->camera, aspect);
        break;

      case 1:
        draw_object2d(scene->objects2d[indices[1]], renderer, scene->camera, aspect);
        break;

      case 2:
        draw_text2d(scene->text2d[indices[2]], renderer, scene->camera, aspect);
        break;

      case 3:
        if (scene->lights3d[indices[3]] && scene->lights3d[indices[3]]->object) {

          draw_object3d(scene->lights3d[indices[3]]->object, renderer, scene->camera, aspect);
        }
        break;

      case 4:
        if (scene->lights2d[indices[4]] && scene->lights2d[indices[4]]->object) {

          draw_object2d(scene->lights2d[indices[4]]->object, renderer, scene->camera, aspect);
        }
        break;
      }

      indices[best]++;
    }
  }

  return VULKAN_RendererEndFrame(renderer);
}
