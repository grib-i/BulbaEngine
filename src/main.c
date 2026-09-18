#include "bulba/core/render/material.h"
#include "debug.h"
#include <bulba/bulba.h>

#include <stdio.h>
#include <time.h>

int main(void) {
  BLB_Window *window = BLB_CreateWindow(1200, 900, "BulbaEngine");
  if (!window) {
    fprintf(stderr, "window failed\n");
    return 1;
  }

  VULKAN vk = {0};
  if (VULKAN_Init(&vk, window->handle) != 0) {
    fprintf(stderr, "vulkan init failed: %s\n", VULKAN_GetLastError(&vk));
    BLB_DestroyWindow(window);
    return 1;
  }

  if (VULKAN_CreateRenderer(&vk) != 0) {
    fprintf(stderr, "renderer failed\n");
    VULKAN_Shutdown(&vk);
    BLB_DestroyWindow(window);
    return 1;
  }

  BLB_Camera *camera = BLB_CreateCamera(HMM_V3(0.0f, 0.0f, 0.0f));
  BLB_Scene *scene = BLB_CreateScene("main");

  if (!camera || !scene) {
    fprintf(stderr, "engine object initialization failed\n");
    BLB_DestroyCamera(camera);
    BLB_DestroyScene(scene);
    VULKAN_Shutdown(&vk);
    BLB_DestroyWindow(window);
    return 1;
  }

  scene->camera = camera;
  BLB_SetSceneLayer(scene, 0);
  BLB_SetSceneEnabled(scene, true);
  BLB_SetSceneVisible(scene, true);
  BLB_SetSceneClear(scene, true, 8, 10, 16, 255);

  // textures
  // BLB_Texture **sprite_list = BLB_SpriteListAuto_Load2D("assets/textures/zta-stones.png", 1);
  // BLB_Texture *sprite = BLB_Texture_Load2D("assets/textures/zta-stones.png");

  // light for 3d
  BLB_SuperObject3D *point_light =
      BLB_CreateSuperLightObject3D(BLB_LIGHT_POINT, HMM_V3(1.0f, 1.0f, 1.0f), HMM_V3(0.0f, 3.5f, -5.0f), HMM_V3(0.0f, -1.0f, 0.0f));

  if (point_light) {
    point_light->light.color = HMM_V3(1.0f, 0.86f, 0.72f);
    point_light->light.intensity = 4.0f;
    point_light->light.ambient = 0.08f;
    point_light->light.range = 20.0f;
    point_light->light.specular = 0.0f;
    point_light->light.enabled = true;
    point_light->object.visible = false;
    BLB_AddSuperLightObject3D(scene, point_light);
  }

  // 2d objests
  BLB_Object2D *square2d = BLB_CreateSquare2D(HMM_V2(100, 100), HMM_V2(200.0f, 100.0f), NULL);
  if (square2d) {
    BLB_Material_SetName(square2d->material, "GlowGreen2D");
    BLB_Material_SetEmission(square2d->material, 0.03f, 1.0f, 0.12f, 1.0f, 0.14f);
    BLB_Material_SetGlow(square2d->material, 0.8f, 22.0f, 2.2f);
    BLB_Material_SetRenderMode(square2d->material, BLB_RENDER_OPAQUE);
    square2d->layer = 50;
    square2d->visible = true;
    square2d->rotation = 0.0f;

    BLB_AddObject2D(scene, square2d);
  }

  BLB_Object2D *circle = BLB_CreateCircle2D(HMM_V2(100, 100), HMM_V2(200.0f, 250.0f), 4, NULL);
  if (circle) {
    BLB_Material_SetName(circle->material, "GlowGreen2D");
    BLB_Material_SetEmission(circle->material, 0.03f, 1.0f, 0.12f, 1.0f, 0.14f);
    BLB_Material_SetGlow(circle->material, 0.8f, 22.0f, 2.2f);
    BLB_Material_SetRenderMode(circle->material, BLB_RENDER_OPAQUE);
    circle->layer = 50;
    circle->visible = true;
    circle->rotation = 0.0f;

    BLB_AddObject2D(scene, circle);
  }

  // 3d objects
  BLB_Object3D *sphere = BLB_CreateSphere3D(HMM_V3(2, 2, 2), HMM_V3(0.0f, 0.0f, -10.0f), 4, NULL);

  if (sphere) {
    BLB_Material_SetName(sphere->material, "GlowGreen2D");
    BLB_Material_SetEmission(sphere->material, 0.03f, 1.0f, 0.12f, 1.0f, 0.14f);
    BLB_Material_SetGlow(sphere->material, 0.8f, 22.0f, 2.2f);
    BLB_Material_SetBaseColor(sphere->material, 1.0f, 0.140f, 0.105f, 1.0f);
    BLB_Material_SetRenderMode(sphere->material, BLB_RENDER_OPAQUE);
    sphere->layer = 1;
    sphere->visible = true;

    BLB_AddObject3D(scene, sphere);
  }

  BLB_Object3D *torus = BLB_CreateTorus3D(HMM_V3(2, 2, 2), HMM_V3(-5.0f, 0.0f, -10.0f), 4, 0.0f, 0.0f, NULL);

  if (torus) {
    BLB_Material_SetName(torus->material, "GlowGreen2D");
    BLB_Material_SetEmission(torus->material, 0.03f, 1.0f, 0.12f, 1.0f, 0.14f);
    BLB_Material_SetGlow(torus->material, 0.8f, 22.0f, 2.2f);
    BLB_Material_SetBaseColor(torus->material, 1.0f, 0.140f, 0.105f, 1.0f);
    BLB_Material_SetRenderMode(torus->material, BLB_RENDER_OPAQUE);
    torus->layer = 1;
    torus->visible = true;

    BLB_AddObject3D(scene, torus);
  }

  BLB_Object3D *cube = BLB_CreateCube3D(HMM_V3(2, 2, 2), HMM_V3(5.0f, 0.0f, -10.0f), NULL);

  if (cube) {
    BLB_Material_SetName(cube->material, "GlowGreen2D");
    BLB_Material_SetEmission(cube->material, 0.03f, 1.0f, 0.12f, 1.0f, 0.14f);
    BLB_Material_SetGlow(cube->material, 0.8f, 0.1f, 2.2f);
    BLB_Material_SetBaseColor(cube->material, 0.127f, 0.181f, 0.181f, 1.0f);
    BLB_Material_SetRenderMode(cube->material, BLB_RENDER_OPAQUE);
    cube->layer = 1;
    cube->visible = true;

    BLB_AddObject3D(scene, cube);
  }

  // fps text
  BLB_Text2D *fps_text = BLB_CreateText2D("FPS: 0.0", NULL, HMM_V2(24.0f, 20.0f), 26.0f);
  if (fps_text) {
    fps_text->color[0] = 255;
    fps_text->color[1] = 255;
    fps_text->color[2] = 255;
    fps_text->color[3] = 255;
    fps_text->visible = true;
    BLB_Material_SetBaseColor(fps_text->material, 1.0f, 1.0f, 1.0f, 1.0f);
    BLB_Material_SetEmission(fps_text->material, 1.0f, 1.0f, 1.0f, 1.0f, 0.2f);
    BLB_Material_SetRenderMode(fps_text->material, BLB_RENDER_TRANSPARENT);
    fps_text->layer = 10;
    BLB_AddText2D(scene, fps_text);
  }

  BLB_FPS fps;
  BLB_InitFPS(&fps);
  char fps_buffer[64];

  int a = 100;
  while (!BLB_WindowShouldClose(window)) {
    BLB_WindowPollEvents(window);

    double now = BLB_Platform_TimeSeconds();
    BLB_UpdateFPS(&fps, now);

    float delta_time = BLB_GetDeltaTime(&fps);
    if (delta_time <= 0.0f)
      delta_time = 1.0f / 60.0f;
    if (delta_time > 0.05f)
      delta_time = 0.05f;

    BLB_SetSceneDeltaTime(scene, delta_time);

    if (fps_text) {
      snprintf(fps_buffer, sizeof(fps_buffer), "FPS: %.1f", BLB_GetFPS(&fps));
      BLB_SetText2D(fps_text, fps_buffer);
    }

    int result = BLB_DrawScene(scene, &vk);
    BLB_Rotate3D(sphere, HMM_V3(a, a, 0));

    BLB_Rotate3D(cube, HMM_V3(a, a, a));

    BLB_Rotate3D(torus, HMM_V3(a, a, 0));

    if (result == -2) {
      if (VULKAN_RendererRecreateSwapchain(&vk) != 0)
        break;
      window->resized = false;
    } else if (result < 0) {
      fprintf(stderr, "draw failed %d\n", result);
      break;
    }
  }

  BLB_DestroySphere3D(sphere);
  BLB_DestroyCube3D(cube);
  BLB_DestroyTorus3D(torus);

  BLB_DestroySquare2D(square2d);
  BLB_DestroyCircle2D(circle);

  BLB_DestroyText2D(fps_text);
  BLB_DestroyScene(scene);
  BLB_DestroyCamera(camera);
  VULKAN_Shutdown(&vk);
  BLB_DestroyWindow(window);
  return 0;
}
