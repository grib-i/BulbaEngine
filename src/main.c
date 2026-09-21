#include <bulba/bulba.h>
#include <debug.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

static void configure_advanced(BLB_Material *m) {
  BLB_Material_SetBaseColor(m, 0.70f, 0.72f, 0.76f, 1.0f);
  BLB_Material_SetPBR(m, 0.15f, 0.45f);
  BLB_Material_SetNormal(m, 1.0f);
  BLB_Material_SetOcclusion(m, 0.9f);
  BLB_Material_SetSpecular(m, 1.0f, 1.0f, 0.96f, 0.92f);
  BLB_Material_SetIOR(m, 1.5f);
  BLB_Material_SetTransmission(m, 0.0f);
  BLB_Material_SetVolume(m, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  BLB_Material_SetClearcoat(m, 0.0f, 0.08f, 1.0f);
  BLB_Material_SetSheen(m, 0.0f, 0.0f, 0.0f, 0.2f);
  BLB_Material_SetIridescence(m, 0.0f, 1.35f, 100.0f, 700.0f);
  BLB_Material_SetAnisotropy(m, 0.0f, 0.0f);
  BLB_Material_SetDispersion(m, 0.0f);
  BLB_Material_SetEmission(m, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
  BLB_Material_SetGlow(m, 0.0f, 0.0f, 2.0f);
  BLB_Material_SetAlphaMode(m, BLB_ALPHA_OPAQUE);
  BLB_Material_SetDepth(m, true, true);
  BLB_Material_SetLighting(m, true);
  BLB_Material_SetDoubleSided(m, false);
  BLB_Material_SetUnlit(m, false);
}

static BLB_Material *make_material(const char *name) {
  BLB_Material *m = BLB_Material_Build(BLB_MATERIAL_3D, configure_advanced);
  if (!m)
    return NULL;
  BLB_Material_SetName(m, name);
  return m;
}

static void replace_material3d(BLB_Object3D *object, BLB_Material *material) {
  if (!object || !material)
    return;
  BLB_Material_Release(object->material);
  object->material = material;
}

static void replace_material2d(BLB_Object2D *object, BLB_Material *material) {
  if (!object || !material)
    return;
  BLB_Material_Release(object->material);
  object->material = material;
}

static BLB_Object3D *make_demo_sphere(HMM_Vec3 position) { return BLB_CreateSphere3D(HMM_V3(2.25f, 2.25f, 2.25f), position, 5, NULL); }

static BLB_Object3D *make_demo_torus(HMM_Vec3 position) { return BLB_CreateTorus3D(HMM_V3(2.25f, 2.25f, 2.25f), position, 5, 0.0f, 0.0f, NULL); }

static BLB_Object3D *make_demo_cube(HMM_Vec3 position) { return BLB_CreateCube3D(HMM_V3(2.35f, 2.35f, 2.35f), position, NULL); }

static BLB_Text2D *make_label(const char *text, HMM_Vec2 position, float size) {
  BLB_Text2D *label = BLB_CreateText2D(text, NULL, position, size, true);
  if (!label)
    return NULL;
  label->color[0] = 235;
  label->color[1] = 235;
  label->color[2] = 245;
  label->color[3] = 255;
  label->layer = 100;
  BLB_Material_SetEmission(label->material, 0.8f, 0.85f, 1.0f, 1.0f, 0.25f);
  BLB_Material_SetRenderMode(label->material, BLB_RENDER_TRANSPARENT);
  return label;
}

int main(void) {
  BLB_DEBUG = true;
  DEBUG_InitStdIO();
  BLB_Init();

  BLB_Window *window = BLB_CreateWindow(1200, 900, "BulbaEngine — Material Lab");
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

  BLB_Camera *camera = BLB_CreateCamera(HMM_V3(0.0f, 0.0f, 15.5f));
  BLB_Scene *scene = BLB_CreateScene("material_lab");
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
  BLB_SetSceneClear(scene, true, 7, 9, 15, 255);

  BLB_Light3D *key = BLB_CreateLight3D(BLB_LIGHT_POINT, HMM_V3(0.0f, 1.0f, 4.0f), HMM_V3(0.0f, 60.0f, 0.0f));
  BLB_Light3D *fill = BLB_CreateLight3D(BLB_LIGHT_POINT, HMM_V3(-6.0f, -2.0f, 2.0f), HMM_V3(0.0f, 5.0f, 0.0f));
  if (key) {
    key->intensity = 13.0f;
    key->ambient = 0.025f;
    key->range = 32.0f;
    key->specular = 1.0f;
    key->enabled = true;
    key->object->layer = 100;
    BLB_AddLight3D(scene, key);
  }
  if (fill) {
    fill->intensity = 6.0f;
    fill->ambient = 0.01f;
    fill->range = 28.0f;
    fill->specular = 0.7f;
    fill->enabled = true;
    fill->object->layer = 100;
    BLB_AddLight3D(scene, fill);
  }

  // 1. metallic + anisotropic metal.
  BLB_Object3D *metal = make_demo_sphere(HMM_V3(-5.0f, 2.4f, 0.0f));
  if (metal) {
    BLB_Material *m = make_material("Metal | metallic + anisotropy");
    BLB_Material_SetBaseColor(m, 0.78f, 0.82f, 0.92f, 1.0f);
    BLB_Material_SetPBR(m, 1.0f, 0.14f);
    BLB_Material_SetSpecular(m, 0.9f, 0.95f, 0.98f, 1.0f);
    BLB_Material_SetAnisotropy(m, 0.82f, 0.35f);
    replace_material3d(metal, m);
    metal->layer = 1;
    BLB_AddObject3D(scene, metal);
  }

  // 2. rough ceramic + normal + AO + sheen.
  BLB_Object3D *ceramic = make_demo_cube(HMM_V3(0.0f, 2.4f, 0.0f));
  if (ceramic) {
    BLB_Material *m = make_material("Ceramic | rough + normal + AO");
    BLB_Material_SetBaseColor(m, 0.18f, 0.38f, 0.62f, 1.0f);
    BLB_Material_SetPBR(m, 0.0f, 0.72f);
    BLB_Material_SetNormal(m, 1.2f);
    BLB_Material_SetOcclusion(m, 0.95f);
    BLB_Material_SetSheen(m, 0.08f, 0.14f, 0.28f, 0.55f);
    BLB_Material_SetClearcoat(m, 0.65f, 0.11f, 1.0f);
    replace_material3d(ceramic, m);
    ceramic->rotation = HMM_V3(8.0f, 25.0f, 8.0f);
    ceramic->layer = 1;
    BLB_AddObject3D(scene, ceramic);
  }

  // 3. glass + transmission + IOR + volume + dispersion.
  BLB_Object3D *glass = make_demo_torus(HMM_V3(5.0f, 2.4f, 0.0f));
  if (glass) {
    BLB_Material *m = make_material("Glass | transmission + IOR + volume + dispersion");
    BLB_Material_SetBaseColor(m, 0.35f, 0.66f, 0.92f, 0.72f);
    BLB_Material_SetPBR(m, 0.0f, 0.10f);
    BLB_Material_SetIOR(m, 1.52f);
    BLB_Material_SetTransmission(m, 0.95f);
    BLB_Material_SetVolume(m, 1.6f, 4.0f, 0.28f, 0.72f, 1.0f);
    BLB_Material_SetDispersion(m, 0.75f);
    BLB_Material_SetAlphaMode(m, BLB_ALPHA_BLEND);
    BLB_Material_SetRenderMode(m, BLB_RENDER_TRANSPARENT);
    BLB_Material_SetDepth(m, true, false);
    replace_material3d(glass, m);
    glass->rotation = HMM_V3(30.0f, 0.0f, 0.0f);
    glass->layer = 2;
    BLB_AddObject3D(scene, glass);
  }

  // 4. car paint + metallic + clearcoat.
  BLB_Object3D *paint = make_demo_sphere(HMM_V3(-5.0f, -2.8f, 0.0f));
  if (paint) {
    BLB_Material *m = make_material("CarPaint | metallic + clearcoat");
    BLB_Material_SetBaseColor(m, 0.62f, 0.025f, 0.035f, 1.0f);
    BLB_Material_SetPBR(m, 0.42f, 0.25f);
    BLB_Material_SetSpecular(m, 1.0f, 1.0f, 0.82f, 0.82f);
    BLB_Material_SetClearcoat(m, 0.97f, 0.055f, 1.0f);
    BLB_Material_SetAnisotropy(m, 0.18f, 1.5f);
    replace_material3d(paint, m);
    paint->layer = 1;
    BLB_AddObject3D(scene, paint);
  }

  // 5. fabric + sheen + normal detail.
  BLB_Object3D *fabric = make_demo_sphere(HMM_V3(0.0f, -2.8f, 0.0f));
  if (fabric) {
    BLB_Material *m = make_material("Fabric | sheen + normal + roughness");
    BLB_Material_SetBaseColor(m, 0.34f, 0.065f, 0.40f, 1.0f);
    BLB_Material_SetPBR(m, 0.0f, 0.92f);
    BLB_Material_SetNormal(m, 1.65f);
    BLB_Material_SetSheen(m, 0.62f, 0.10f, 0.82f, 0.38f);
    BLB_Material_SetOcclusion(m, 0.82f);
    replace_material3d(fabric, m);
    fabric->layer = 1;
    BLB_AddObject3D(scene, fabric);
  }

  // 6. iridescent + anisotropic + emission/glow.
  BLB_Object3D *iridescent = make_demo_torus(HMM_V3(5.0f, -2.8f, 0.0f));
  if (iridescent) {
    BLB_Material *m = make_material("Iridescent | thin film + anisotropy + emission");
    BLB_Material_SetBaseColor(m, 0.08f, 0.12f, 0.20f, 1.0f);
    BLB_Material_SetPBR(m, 0.82f, 0.19f);
    BLB_Material_SetIridescence(m, 1.0f, 1.38f, 140.0f, 900.0f);
    BLB_Material_SetAnisotropy(m, 0.88f, 0.95f);
    BLB_Material_SetDispersion(m, 0.55f);
    BLB_Material_SetEmission(m, 0.10f, 0.34f, 1.0f, 1.0f, 0.22f);
    BLB_Material_SetTemperature(m, 9000.0f);
    BLB_Material_SetGlow(m, 0.85f, 18.0f, 2.35f);
    replace_material3d(iridescent, m);
    iridescent->rotation = HMM_V3(20.0f, 20.0f, 0.0f);
    iridescent->layer = 1;
    BLB_AddObject3D(scene, iridescent);
  }

  // a compact 2d material demo without test textures.
  BLB_Object2D *ui_glow = BLB_CreateCircle2D(HMM_V2(1.0f, 1.0f), HMM_V2(1060.0f, 700.0f), 5, NULL, true);
  if (ui_glow) {
    BLB_Material *m = BLB_Material_Build(BLB_MATERIAL_2D, NULL);
    if (m) {
      BLB_Material_SetName(m, "2D | unlit + emission + glow");
      BLB_Material_SetBaseColor(m, 0.05f, 0.25f, 0.95f, 1.0f);
      BLB_Material_SetEmission(m, 0.12f, 0.42f, 1.0f, 1.0f, 1.2f);
      BLB_Material_SetGlow(m, 1.0f, 28.0f, 2.0f);
      BLB_Material_SetLighting(m, false);
      BLB_Material_SetUnlit(m, true);
      BLB_Material_SetDepth(m, false, false);
      BLB_Material_SetDoubleSided(m, true);
      BLB_Material_SetAlphaMode(m, BLB_ALPHA_BLEND);
      BLB_Material_SetRenderMode(m, BLB_RENDER_TRANSPARENT);
      replace_material2d(ui_glow, m);
    }
    ui_glow->scale = HMM_V2(130.0f, 130.0f);
    ui_glow->layer = 80;
    BLB_AddObject2D(scene, ui_glow);
  }

  BLB_Text2D *title = make_label("BULBA MATERIAL LAB", HMM_V2(28.0f, 22.0f), 30.0f);
  BLB_Text2D *top_info = make_label("METAL / NORMAL+AO / GLASS / CLEARCOAT", HMM_V2(28.0f, 58.0f), 18.0f);
  BLB_Text2D *bottom_info = make_label("FABRIC / SHEEN / IRIDESCENCE / ANISOTROPY / DISPERSION", HMM_V2(28.0f, 82.0f), 18.0f);
  BLB_Text2D *ui_info = make_label("2D UNLIT / EMISSION + GLOW", HMM_V2(880.0f, 785.0f), 18.0f);
  BLB_Text2D *fps_text = make_label("FPS: 0", HMM_V2(28.0f, 850.0f), 18.0f);

  if (title)
    BLB_AddText2D(scene, title);
  if (top_info)
    BLB_AddText2D(scene, top_info);
  if (bottom_info)
    BLB_AddText2D(scene, bottom_info);
  if (ui_info)
    BLB_AddText2D(scene, ui_info);
  if (fps_text)
    BLB_AddText2D(scene, fps_text);

  BLB_FPS fps;
  BLB_InitFPS(&fps, &vk, 0, false);
  char fps_buffer[64];
  float time = 0.0f;

  while (!BLB_WindowShouldClose(window)) {
    BLB_WindowPollEvents(window);
    BLB_UpdateFPS(&fps);

    if (window->resized) {
      if (!BLB_WindowResizeStable(window, 0.08))
        continue;

      int resize_result = VULKAN_RendererRecreateSwapchain(&vk, vk.vsync_enabled);
      if (resize_result == 1)
        continue;
      if (resize_result != 0)
        break;

      window->resized = false;
      continue;
    }

    float delta_time = BLB_GetDeltaTime(&fps);
    if (delta_time <= 0.0f)
      delta_time = 1.0f / 60.0f;
    if (delta_time > 0.05f)
      delta_time = 0.05f;
    time += delta_time;

    BLB_SetSceneDeltaTime(scene, delta_time);

    if (fps_text) {
      snprintf(fps_buffer, sizeof(fps_buffer), "FPS: %d", (int)BLB_GetFPS(&fps));
      BLB_SetText2D(fps_text, fps_buffer);
    }

    if (key) {
      const HMM_Vec3 p = HMM_V3(cosf(time * 0.72f) * 8.0f, sinf(time * 0.95f) * 4.0f, 4.5f);
      BLB_Object3D_SetPosition(key->object, p);
    }
    if (fill) {
      const HMM_Vec3 p = HMM_V3(sinf(time * 0.48f) * 7.0f, cosf(time * 0.63f) * 3.5f, 2.0f);
      BLB_Object3D_SetPosition(fill->object, p);
    }

    if (metal)
      BLB_Object3D_Rotate(metal, HMM_V3(0.0f, 22.0f, 0.0f));
    if (ceramic)
      BLB_Object3D_Rotate(ceramic, HMM_V3(10.0f, 16.0f, 5.0f));
    if (glass)
      BLB_Object3D_Rotate(glass, HMM_V3(18.0f, 24.0f, 0.0f));
    if (paint)
      BLB_Object3D_Rotate(paint, HMM_V3(0.0f, 18.0f, 0.0f));
    if (fabric)
      BLB_Object3D_Rotate(fabric, HMM_V3(11.0f, 13.0f, 0.0f));
    if (iridescent)
      BLB_Object3D_Rotate(iridescent, HMM_V3(12.0f, 26.0f, 8.0f));

    int result = BLB_DrawScene(scene, &vk);
    if (result == -2) {
      window->resized = true;
      window->resize_time = BLB_Platform_TimeSeconds();
    } else if (result < 0) {
      fprintf(stderr, "draw failed %d\n", result);
      break;
    }
  }

  BLB_DestroySphere3D(metal);
  BLB_DestroyCube3D(ceramic);
  BLB_DestroyTorus3D(glass);
  BLB_DestroySphere3D(paint);
  BLB_DestroySphere3D(fabric);
  BLB_DestroyTorus3D(iridescent);
  BLB_DestroyCircle2D(ui_glow);

  BLB_DestroyText2D(title);
  BLB_DestroyText2D(top_info);
  BLB_DestroyText2D(bottom_info);
  BLB_DestroyText2D(ui_info);
  BLB_DestroyText2D(fps_text);

  BLB_DestroyLight3D(key);
  BLB_DestroyLight3D(fill);
  BLB_DestroyScene(scene);
  BLB_DestroyCamera(camera);
  VULKAN_Shutdown(&vk);
  BLB_DestroyWindow(window);
  return 0;
}
