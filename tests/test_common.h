#ifndef BLB_TEST_COMMON_H
#define BLB_TEST_COMMON_H

#include <bulba/bulba.h>
#include <debug.h>

#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define BLB_TEST_MAX_LIGHTS 8

typedef struct {
  BLB_Window *window;
  VULKAN vk;
  BLB_Camera *camera;
  BLB_Scene *scene;
  BLB_FPS fps;
  BLB_Text2D *fps_text;
  BLB_Light3D *lights[BLB_TEST_MAX_LIGHTS];
  size_t light_count;
  float fps_timer;
  char fps_buffer[32];
} BLB_TestContext;

static inline int BLB_TestContext_Init(BLB_TestContext *ctx, int width, int height, const char *title, HMM_Vec3 camera_position,
                                       unsigned char clear_r, unsigned char clear_g, unsigned char clear_b) {
  if (!ctx)
    return -1;

  memset(ctx, 0, sizeof(*ctx));

  ctx->window = BLB_CreateWindow(width, height, title);
  if (!ctx->window)
    return -1;

  if (VULKAN_Init(&ctx->vk, ctx->window->handle) != 0) {
    BLB_DestroyWindow(ctx->window);
    ctx->window = NULL;
    return -1;
  }

  if (VULKAN_CreateRenderer(&ctx->vk) != 0) {
    VULKAN_Shutdown(&ctx->vk);
    BLB_DestroyWindow(ctx->window);
    ctx->window = NULL;
    return -1;
  }

  ctx->camera = BLB_CreateCamera(camera_position);
  ctx->scene = BLB_CreateScene(title ? title : "test");

  if (!ctx->camera || !ctx->scene) {
    BLB_DestroyCamera(ctx->camera);
    BLB_DestroyScene(ctx->scene);
    ctx->camera = NULL;
    ctx->scene = NULL;
    VULKAN_Shutdown(&ctx->vk);
    BLB_DestroyWindow(ctx->window);
    ctx->window = NULL;
    return -1;
  }

  ctx->scene->camera = ctx->camera;
  BLB_SetSceneLayer(ctx->scene, 0);
  BLB_SetSceneEnabled(ctx->scene, true);
  BLB_SetSceneVisible(ctx->scene, true);
  BLB_SetSceneClear(ctx->scene, true, clear_r, clear_g, clear_b, 255);

  ctx->fps_text = BLB_CreateText2D("FPS: 0", NULL, HMM_V2(18.0f, 18.0f), 18.0f, true);
  if (ctx->fps_text) {
    ctx->fps_text->layer = 1000;
    ctx->fps_text->color[0] = 235;
    ctx->fps_text->color[1] = 235;
    ctx->fps_text->color[2] = 245;
    ctx->fps_text->color[3] = 255;
    BLB_Material_SetLighting(ctx->fps_text->material, false);
    BLB_Material_SetUnlit(ctx->fps_text->material, true);
    BLB_Material_SetDepth(ctx->fps_text->material, false, false);
    BLB_Material_SetDoubleSided(ctx->fps_text->material, true);
    BLB_AddText2D(ctx->scene, ctx->fps_text);
  }

  BLB_InitFPS(&ctx->fps, &ctx->vk, 0, false);
  return 0;
}

static inline BLB_Light3D *BLB_TestContext_AddLight(BLB_TestContext *ctx, BLB_LightType type, HMM_Vec3 position, HMM_Vec3 rotation,
                                                     float intensity, float ambient, float specular, float range) {
  if (!ctx || !ctx->scene || ctx->light_count >= BLB_TEST_MAX_LIGHTS)
    return NULL;

  BLB_Light3D *light = BLB_CreateLight3D(type, position, rotation);
  if (!light)
    return NULL;

  light->intensity = intensity;
  light->ambient = ambient;
  light->specular = specular;
  light->range = range;
  light->enabled = true;

  if (BLB_AddLight3D(ctx->scene, light) != 0) {
    BLB_DestroyLight3D(light);
    return NULL;
  }

  ctx->lights[ctx->light_count++] = light;
  return light;
}

static inline int BLB_TestContext_BeginFrame(BLB_TestContext *ctx, float *delta_time) {
  if (!ctx || !ctx->window || !ctx->scene || !delta_time)
    return -1;

  BLB_WindowPollEvents(ctx->window);
  BLB_UpdateFPS(&ctx->fps);

  if (ctx->window->resized) {
    if (!BLB_WindowResizeStable(ctx->window, 0.08))
      return 1;

    int resize_result = VULKAN_RendererRecreateSwapchain(&ctx->vk, ctx->vk.vsync_enabled);
    if (resize_result == 1)
      return 1;
    if (resize_result != 0)
      return -1;

    ctx->window->resized = false;
    return 1;
  }

  float dt = BLB_GetDeltaTime(&ctx->fps);
  if (dt <= 0.0f)
    dt = 1.0f / 60.0f;
  if (dt > 0.05f)
    dt = 0.05f;

  ctx->fps_timer += dt;
  BLB_SetSceneDeltaTime(ctx->scene, dt);

  if (ctx->fps_text && ctx->fps_timer >= 0.25f) {
    ctx->fps_timer = 0.0f;
    snprintf(ctx->fps_buffer, sizeof(ctx->fps_buffer), "FPS: %d", (int)BLB_GetFPS(&ctx->fps));
    BLB_SetText2D(ctx->fps_text, ctx->fps_buffer);
  }

  *delta_time = dt;
  return 0;
}

static inline int BLB_TestContext_Draw(BLB_TestContext *ctx) {
  if (!ctx || !ctx->window || !ctx->scene)
    return -1;

  int result = BLB_DrawScene(ctx->scene, &ctx->vk);
  if (result == -2) {
    ctx->window->resized = true;
    ctx->window->resize_time = BLB_Platform_TimeSeconds();
    return 0;
  }

  return result;
}

static inline void BLB_TestContext_Shutdown(BLB_TestContext *ctx) {
  if (!ctx)
    return;

  for (size_t i = 0; i < ctx->light_count; ++i)
    BLB_DestroyLight3D(ctx->lights[i]);
  ctx->light_count = 0;

  BLB_DestroyText2D(ctx->fps_text);
  ctx->fps_text = NULL;

  BLB_DestroyScene(ctx->scene);
  ctx->scene = NULL;

  BLB_DestroyCamera(ctx->camera);
  ctx->camera = NULL;

  VULKAN_Shutdown(&ctx->vk);

  BLB_DestroyWindow(ctx->window);
  ctx->window = NULL;
}

static inline void BLB_TestAddStudioLights(BLB_TestContext *ctx, float range) {
  BLB_TestContext_AddLight(ctx, BLB_LIGHT_POINT, HMM_V3(0.0f, 4.5f, 6.0f), HMM_V3(0.0f, 0.0f, 0.0f), 13.0f, 0.025f, 1.0f, range);
  BLB_TestContext_AddLight(ctx, BLB_LIGHT_POINT, HMM_V3(-6.0f, -2.0f, 4.0f), HMM_V3(0.0f, 0.0f, 0.0f), 6.0f, 0.01f, 0.7f, range);
}

#endif
