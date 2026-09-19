#include "bulba/core/render.h"

#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/math3v/lights.h"
#include "bulba/core/objects3d/objects3d.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void rgb(const unsigned char color[4], float output[4]) {
  output[0] = color[0] / 255.0f;
  output[1] = color[1] / 255.0f;
  output[2] = color[2] / 255.0f;
  output[3] = color[3] / 255.0f;
}

static BLB_RenderMode normalize_render_mode(BLB_RenderMode mode) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    return BLB_RENDER_OPAQUE;

  return mode;
}

static int compare_object3d(const void *a, const void *b) {
  BLB_Object3D *aa = *(BLB_Object3D **)a;
  BLB_Object3D *bb = *(BLB_Object3D **)b;

  if (!aa || !bb)
    return aa ? -1 : bb ? 1 : 0;

  BLB_RenderMode am = aa->material ? aa->material->render_mode : aa->render_mode;

  BLB_RenderMode bm = bb->material ? bb->material->render_mode : bb->render_mode;

  am = normalize_render_mode(am);
  bm = normalize_render_mode(bm);

  if (am != bm)
    return (int)am - (int)bm;

  return (int)aa->layer - (int)bb->layer;
}

static int compare_object2d(const void *a, const void *b) {
  BLB_Object2D *aa = *(BLB_Object2D **)a;
  BLB_Object2D *bb = *(BLB_Object2D **)b;

  if (!aa || !bb)
    return aa ? -1 : bb ? 1 : 0;

  BLB_RenderMode am = aa->material ? aa->material->render_mode : aa->render_mode;

  BLB_RenderMode bm = bb->material ? bb->material->render_mode : bb->render_mode;

  am = normalize_render_mode(am);
  bm = normalize_render_mode(bm);

  if (am != bm)
    return (int)am - (int)bm;

  return (int)aa->layer - (int)bb->layer;
}

static int compare_text2d(const void *a, const void *b) {
  BLB_Text2D *aa = *(BLB_Text2D **)a;
  BLB_Text2D *bb = *(BLB_Text2D **)b;

  if (!aa || !bb)
    return aa ? -1 : bb ? 1 : 0;

  BLB_RenderMode am = aa->material ? aa->material->render_mode : aa->render_mode;

  BLB_RenderMode bm = bb->material ? bb->material->render_mode : bb->render_mode;

  am = normalize_render_mode(am);
  bm = normalize_render_mode(bm);

  if (am != bm)
    return (int)am - (int)bm;

  return (int)aa->layer - (int)bb->layer;
}

static int compare_light3d(const void *a, const void *b) {
  BLB_Light3D *aa = *(BLB_Light3D **)a;
  BLB_Light3D *bb = *(BLB_Light3D **)b;

  if (!aa || !bb)
    return aa ? -1 : bb ? 1 : 0;

  if (!aa->object || !bb->object)
    return aa->object ? -1 : bb->object ? 1 : 0;

  return compare_object3d(&aa->object, &bb->object);
}

static int compare_light2d(const void *a, const void *b) {
  BLB_Light2D *aa = *(BLB_Light2D **)a;
  BLB_Light2D *bb = *(BLB_Light2D **)b;

  if (!aa || !bb)
    return aa ? -1 : bb ? 1 : 0;

  if (!aa->object || !bb->object)
    return aa->object ? -1 : bb->object ? 1 : 0;

  return compare_object2d(&aa->object, &bb->object);
}

static void *get_render_object(BLB_Scene *scene, int type, size_t index) {
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
  }

  return NULL;
}

static BLB_RenderMode get_render_mode(void *object, int type) {
  if (!object)
    return BLB_RENDER_OPAQUE;

  if (type == 0 || type == 3) {
    BLB_Object3D *value = object;

    return normalize_render_mode(value->material ? value->material->render_mode : value->render_mode);
  }

  if (type == 1 || type == 4) {
    BLB_Object2D *value = object;

    return normalize_render_mode(value->material ? value->material->render_mode : value->render_mode);
  }

  BLB_Text2D *value = object;

  return normalize_render_mode(value->material ? value->material->render_mode : value->render_mode);
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

    if (!candidate)
      continue;

    if (!candidate->enabled)
      continue;

    if (candidate->type != BLB_LIGHT_DIRECTIONAL)
      continue;

    light = candidate;
    break;
  }

  if (!light)
    return false;

  HMM_Vec3 direction = BLB_GetLightDirection3D(light);

  float target_z = -20.0f;

  if (scene->object3d_count > 0) {
    HMM_Vec3 target = HMM_V3(0.0f, 0.0f, 0.0f);

    int count = 0;

    for (int i = 0; i < scene->object3d_count; i++) {
      BLB_Object3D *object = scene->objects3d[i];

      if (!object || !object->visible)
        continue;

      target = HMM_AddV3(target, object->position);

      count++;
    }

    if (count > 0) {
      target_z = HMM_MulV3F(target, 1.0f / (float)count).z;
    }
  }

  HMM_Vec3 target = HMM_V3(0.0f, 0.0f, target_z);

  HMM_Vec3 eye = HMM_SubV3(target, HMM_MulV3F(direction, 60.0f));

  HMM_Vec3 up = fabsf(HMM_DotV3(direction, HMM_V3(0.0f, 1.0f, 0.0f))) > 0.98f ? HMM_V3(0.0f, 0.0f, 1.0f) : HMM_V3(0.0f, 1.0f, 0.0f);

  HMM_Mat4 view = HMM_LookAt_RH(eye, target, up);

  HMM_Mat4 projection = HMM_Orthographic_RH_ZO(-35.0f, 35.0f, 35.0f, -35.0f, 0.1f, 140.0f);

  *shadow_vp = HMM_MulM4(projection, view);

  return true;
}

static VulkanMaterial vulkan_material_from(const BLB_Material *material, bool lighting) {
  VulkanMaterial result = {0};

  result.lighting_enabled = lighting && material && material->lighting_enabled;

  result.emission = material ? material->emission_strength : 0.0f;

  result.glow = material ? material->glow_strength : 0.0f;

  result.roundness = material ? material->roughness : 0.0f;

  result.glow_radius = material ? material->glow_radius : 0.0f;

  result.glow_falloff = material ? material->glow_falloff : 0.0f;

  result.render_mode = normalize_render_mode(material ? material->render_mode : BLB_RENDER_OPAQUE);

  return result;
}

static BLB_Material material_fallback3d(const BLB_Object3D *object) {
  BLB_Material material = {0};

  material.domain = BLB_MATERIAL_3D;

  material.render_mode = normalize_render_mode(object->render_mode);

  material.lighting_enabled = true;
  material.depth_enabled = true;

  rgb(object->color, material.base_color);

  material.emission_strength = object->emission;

  material.glow_strength = object->glow;

  material.glow_radius = object->glow > 0.0f ? 1.0f : 0.0f;

  material.glow_falloff = 2.0f;
  material.roughness = object->roundness;

  return material;
}

static BLB_Material material_fallback2d(const BLB_Object2D *object) {
  BLB_Material material = {0};

  material.domain = BLB_MATERIAL_2D;

  material.render_mode = normalize_render_mode(object->render_mode);

  material.lighting_enabled = false;
  material.depth_enabled = false;

  rgb(object->color, material.base_color);

  material.emission_strength = object->emission;

  material.glow_strength = object->glow;

  material.glow_radius = object->glow > 0.0f ? 18.0f : 0.0f;

  material.glow_falloff = 2.0f;
  material.roughness = object->roundness;

  return material;
}

static void draw_object3d_pass(BLB_Object3D *object, VULKAN *renderer, BLB_Camera *camera, float aspect, const BLB_Material *material,
                               float scale_mul, float glow_mul) {
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

  BLB_Material pass = *material;

  pass.emission_strength *= glow_mul;
  pass.glow_strength *= glow_mul;
  pass.base_color[3] *= glow_mul;

  VulkanMaterial vk_material = vulkan_material_from(&pass, true);

  VULKAN_RendererDrawPolygon3D(renderer, object->polygon, &mvp.Elements[0][0], model_rows, normal_rows, pass.base_color[0], pass.base_color[1],
                               pass.base_color[2], pass.base_color[3], &vk_material, object->texture);
}

static void draw_object3d(BLB_Object3D *object, VULKAN *renderer, BLB_Camera *camera, float aspect) {
  if (!object || !renderer || !camera || !object->visible || !object->polygon)
    return;

  BLB_Material fallback;

  const BLB_Material *material = object->material;

  if (!material) {
    fallback = material_fallback3d(object);

    material = &fallback;
  }

  draw_object3d_pass(object, renderer, camera, aspect, material, 1.0f, 1.0f);

  if (material->glow_strength <= 0.0f || material->glow_radius <= 0.0f)
    return;

  float radius = material->glow_radius;

  const float steps = 6.0f;

  for (int i = 1; i <= 6; i++) {
    float t = (float)i / steps;

    float envelope = powf(fmaxf(0.0f, 1.0f - t), fmaxf(material->glow_falloff, 0.2f));

    float scale_mul = 1.0f + radius * 0.035f * t;

    draw_object3d_pass(object, renderer, camera, aspect, material, scale_mul, envelope * material->glow_strength * 0.32f);
  }
}

static void draw_object2d_pass(BLB_Object2D *object, VULKAN *renderer, BLB_Camera *camera, float aspect, const BLB_Material *material,
                               float scale_mul, float glow_mul) {
  BLB_Material pass = *material;

  pass.emission_strength *= glow_mul;
  pass.glow_strength *= glow_mul;
  pass.base_color[3] *= glow_mul;

  VulkanMaterial vk_material = vulkan_material_from(&pass, false);

  float viewport_width = (float)renderer->swapchain_extent.width;

  float viewport_height = (float)renderer->swapchain_extent.height;

  size_t count = object->polygon->vertex_count;

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

    float ndc_x = clip.x / clip.w;

    float ndc_y = clip.y / clip.w;

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

  BLB_Material fallback;

  const BLB_Material *material = object->material;

  if (!material) {
    fallback = material_fallback2d(object);

    material = &fallback;
  }

  draw_object2d_pass(object, renderer, camera, aspect, material, 1.0f, 1.0f);

  if (material->glow_strength <= 0.0f || material->glow_radius <= 0.0f)
    return;

  float base_size = fmaxf(fabsf(object->scale.x), fmaxf(fabsf(object->scale.y), 1.0f));

  for (int i = 1; i <= 7; i++) {
    float t = (float)i / 7.0f;

    float envelope = powf(fmaxf(0.0f, 1.0f - t), fmaxf(material->glow_falloff, 0.2f));

    float scale_mul = 1.0f + (material->glow_radius / base_size) * t;

    draw_object2d_pass(object, renderer, camera, aspect, material, scale_mul, envelope * material->glow_strength * 0.22f);
  }
}

static void draw_text2d(BLB_Text2D *text, VULKAN *renderer, BLB_Camera *camera, float aspect) {
  if (!text || !renderer || !text->visible || !text->text || !text->font_path || !text->font_loaded)
    return;

  (void)camera;
  (void)aspect;

  if (renderer->loaded_font != &text->font) {
    if (VULKAN_RendererLoadFont(renderer, &text->font) != 0)
      return;
  }

  BLB_Material fallback = {0};

  const BLB_Material *material = text->material;

  if (!material) {
    fallback.domain = BLB_MATERIAL_2D;

    fallback.base_color[0] = text->color[0] / 255.0f;

    fallback.base_color[1] = text->color[1] / 255.0f;

    fallback.base_color[2] = text->color[2] / 255.0f;

    fallback.base_color[3] = text->color[3] / 255.0f;

    fallback.emission_strength = text->emission;

    fallback.glow_strength = text->glow;

    fallback.roughness = text->roundness;

    fallback.render_mode = normalize_render_mode(text->render_mode);

    material = &fallback;
  }

  float glyph_scale = text->font.size > 0 ? text->size / (float)text->font.size : 1.0f;

  if (glyph_scale <= 0.0f)
    glyph_scale = 0.001f;

  VULKAN_RendererDrawText(renderer, &text->font, text->text, text->position.x, text->position.y, glyph_scale, text->scale, 0.0f,
                          material->base_color[0], material->base_color[1], material->base_color[2], material->base_color[3],
                          material->emission_strength, material->glow_strength, material->roughness, normalize_render_mode(material->render_mode));
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

  if (scene->camera && scene->camera->delta_time) {
    *scene->camera->delta_time = scene->delta_time;
  }

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

        if (object && object->delta_time) {
          *object->delta_time = scene->delta_time;
        }
      }

      if (i < scene->object2d_count) {
        BLB_Object2D *object = scene->objects2d[i];

        if (object && object->delta_time) {
          *object->delta_time = scene->delta_time;
        }
      }

      if (i < scene->text2d_count) {
        BLB_Text2D *text = scene->text2d[i];

        if (text && text->delta_time) {
          *text->delta_time = scene->delta_time;
        }
      }

      if (i < scene->light3d_count) {
        BLB_Light3D *light = scene->lights3d[i];

        if (light && light->object && light->object->delta_time) {
          *light->object->delta_time = scene->delta_time;
        }
      }

      if (i < scene->light2d_count) {
        BLB_Light2D *light = scene->lights2d[i];

        if (light && light->object && light->object->delta_time) {
          *light->object->delta_time = scene->delta_time;
        }
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

      if (!object || !object->visible || !object->polygon || object->render_mode != BLB_RENDER_OPAQUE)
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
    if (scene->object3d_count > 1) {
      qsort(scene->objects3d, scene->object3d_count, sizeof(BLB_Object3D *), compare_object3d);
    }

    if (scene->object2d_count > 1) {
      qsort(scene->objects2d, scene->object2d_count, sizeof(BLB_Object2D *), compare_object2d);
    }

    if (scene->text2d_count > 1) {
      qsort(scene->text2d, scene->text2d_count, sizeof(BLB_Text2D *), compare_text2d);
    }

    if (scene->light3d_count > 1) {
      qsort(scene->lights3d, scene->light3d_count, sizeof(BLB_Light3D *), compare_light3d);
    }

    if (scene->light2d_count > 1) {
      qsort(scene->lights2d, scene->light2d_count, sizeof(BLB_Light2D *), compare_light2d);
    }

    float aspect = renderer->swapchain_extent.height ? (float)renderer->swapchain_extent.width / (float)renderer->swapchain_extent.height : 1.0f;

    size_t indices[5] = {0, 0, 0, 0, 0};

    size_t counts[5] = {(size_t)scene->object3d_count, (size_t)scene->object2d_count, (size_t)scene->text2d_count, (size_t)scene->light3d_count,
                        (size_t)scene->light2d_count};

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
