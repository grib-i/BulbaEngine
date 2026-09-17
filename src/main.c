#include "debug.h"
#include <bulba/bulba.h>

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

  // textures
  BLB_Texture **sprite_list = BLB_SpriteListAuto_Load2D("assets/textures/zta-stones.png", 1);
  BLB_Texture *sprite = BLB_Texture_Load2D("assets/textures/zta-stones.png");

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
  BLB_Object2D *test_squares[11];

  for (int i = 0; i < 5; i++) {
    HMM_Vec2 size = HMM_V2((float)sprite_list[i]->width / 3, (float)sprite_list[i]->height / 3);
    test_squares[i] = BLB_CreateCube2D(size, HMM_V2(220.0f * (i + 1), 300.0f), sprite_list[i]);

    if (test_squares[i]) {
      BLB_Material_SetName(test_squares[i]->material, "GlowGreen2D");
      BLB_Material_SetEmission(test_squares[i]->material, 0.03f, 1.0f, 0.12f, 1.0f, 0.14f);
      BLB_Material_SetGlow(test_squares[i]->material, 0.8f, 22.0f, 2.2f);
      BLB_Material_SetRenderMode(test_squares[i]->material, BLB_RENDER_OPAQUE);
      test_squares[i]->layer = 50;
      test_squares[i]->visible = true;
      test_squares[i]->rotation = 0.0f;

      BLB_AddObject2D(scene, test_squares[i]);
    }
  }

  for (int i = 5; i < 12; i++) {
    HMM_Vec2 size = HMM_V2((float)sprite_list[i]->width / 3, (float)sprite_list[i]->height / 3);

    test_squares[i] = BLB_CreateCube2D(size, HMM_V2(220.0f * (i - 4), 600.0f), sprite_list[i]);

    if (test_squares[i]) {
      BLB_Material_SetName(test_squares[i]->material, "GlowGreen2D");
      BLB_Material_SetEmission(test_squares[i]->material, 0.03f, 1.0f, 0.12f, 1.0f, 0.14f);
      BLB_Material_SetGlow(test_squares[i]->material, 0.8f, 22.0f, 2.2f);
      BLB_Material_SetRenderMode(test_squares[i]->material, BLB_RENDER_OPAQUE);
      test_squares[i]->layer = 50;
      test_squares[i]->visible = true;
      test_squares[i]->rotation = 0.0f;

      BLB_AddObject2D(scene, test_squares[i]);
    }
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

  if (!sprite_list)
    printd("TEXTURE LOAD FAILED\n");
  else
    printd("TEXTURE: %ux%u\n", BLB_Texture_GetWidth(sprite_list[0]), BLB_Texture_GetHeight(sprite_list[0]));
  BLB_SpriteList_Destroy(sprite_list);

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

    if (result == -2) {
      if (VULKAN_RendererRecreateSwapchain(&vk) != 0)
        break;
      window->resized = false;
    } else if (result < 0) {
      fprintf(stderr, "draw failed %d\n", result);
      break;
    }
  }
  for (int i = 0; i < 11; i++) {
    if (test_squares[i]) {
      BLB_DestroyCube2D(test_squares[i]);
    }
  }

  BLB_DestroyText2D(fps_text);
  BLB_DestroyScene(scene);
  BLB_DestroyCamera(camera);
  VULKAN_Shutdown(&vk);
  BLB_DestroyWindow(window);
  return 0;
}
