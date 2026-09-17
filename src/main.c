#include <bulba/bulba.h>

#include "bulba/core/platform.h"

#include <stdio.h>

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

  // 3d objest
  BLB_Object3D *cube1 = BLB_CreateCube3D(HMM_V3(2.8f, 2.8f, 2.8f), HMM_V3(-3.4f, 0.0f, -9.0f), NULL);

  BLB_Object3D *cube2 = BLB_CreateCube3D(HMM_V3(2.8f, 2.8f, 2.8f), HMM_V3(0.0f, 0.0f, -20.0f), NULL);

  if (cube1) {
    BLB_Material_SetName(cube1->material, "GlowRed3D");
    BLB_Material_SetBaseColor(cube1->material, 0.80f, 0.08f, 0.08f, 1.0f);
    BLB_Material_SetEmission(cube1->material, 1.0f, 0.08f, 0.03f, 1.0f, 0.18f);
    BLB_Material_SetGlow(cube1->material, 100.0f, 1.8f, 22.0f);
    BLB_Material_SetRenderMode(cube1->material, BLB_RENDER_OPAQUE);
    cube1->layer = 10;
    cube1->visible = true;
    BLB_AddObject3D(scene, cube1);
  }

  if (cube2) {
    BLB_Material_SetName(cube2->material, "GlowBlue3D");
    BLB_Material_SetBaseColor(cube2->material, 0.05f, 0.20f, 0.92f, 1.0f);
    BLB_Material_SetEmission(cube2->material, 0.04f, 0.22f, 1.0f, 1.0f, 0.22f);
    BLB_Material_SetGlow(cube2->material, 1.15f, 2.0f, 2.1f);
    BLB_Material_SetRenderMode(cube2->material, BLB_RENDER_OPAQUE);
    cube2->layer = 11;
    cube2->visible = true;
    BLB_AddObject3D(scene, cube2);
  }

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
  BLB_Object2D *test_square = BLB_CreateCube2D(HMM_V2(180.0f, 180.0f), HMM_V2(100.0f, 120.0f), NULL);
  if (test_square) {
    BLB_Material_SetName(test_square->material, "GlowGreen2D");
    BLB_Material_SetBaseColor(test_square->material, 0.03f, 0.86f, 0.18f, 1.0f);
    BLB_Material_SetEmission(test_square->material, 0.03f, 1.0f, 0.12f, 1.0f, 0.14f);
    BLB_Material_SetGlow(test_square->material, 0.8f, 22.0f, 2.2f);
    BLB_Material_SetRenderMode(test_square->material, BLB_RENDER_OPAQUE);
    test_square->layer = 50;
    test_square->visible = true;
    test_square->rotation = 0.0f;
    BLB_AddObject2D(scene, test_square);
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
    fps_text->layer = 1000;
    BLB_AddText2D(scene, fps_text);
  }

  BLB_FPS fps;
  BLB_InitFPS(&fps);
  char fps_buffer[64];

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

    if (cube1)
      BLB_Rotate(cube1, HMM_V3(0.0f, 24.0f, 0.0f));

    if (cube2)
      BLB_Rotate(cube2, HMM_V3(14.0f, -20.0f, 0.0f));

    int result = BLB_DrawScene(scene, &vk);

    if (result == -2) {
      if (VULKAN_RendererRecreateSwapchain(&vk) != 0)
        break;
      window->resized = false;
    } else if (result < 0) {
      fprintf(stderr, "draw failed %d\n", result);
      break;
    }
  }

  BLB_DestroyText2D(fps_text);
  BLB_DestroyCube2D(test_square);
  BLB_DestroyCube3D(cube1);
  BLB_DestroyCube3D(cube2);
  BLB_DestroyScene(scene);
  BLB_DestroyCamera(camera);
  VULKAN_Shutdown(&vk);
  BLB_DestroyWindow(window);
  return 0;
}
