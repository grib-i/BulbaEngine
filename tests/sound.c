#include "bulba/core/camera.h"
#include "bulba/core/math3v/math3v.h"
#include "bulba/core/objects3d/animation3d.h"
#include "bulba/core/objects3d/audio3d.h"
#include "bulba/core/objects3d/model_loader_assimp.h"
#include "bulba/core/objects3d/model_loader_fbx.h"
#include "bulba/core/objects3d/model_loader_glb.h"
#include "bulba/core/objects3d/model_loader_obj.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/scene.h"
#include "test_common.h"
#include "tests.h"

#include <string.h>
#include <unistd.h>

static const char *sound_test_animation_clip(const BLB_Object3D *model) {
  const size_t count = BLB_Model_GetAnimationCount(model);
  const char *first = NULL;
  for (size_t i = 0; i < count; ++i) {
    const char *name = BLB_Model_GetAnimationName(model, i);
    if (!name)
      continue;
    if (!first)
      first = name;
    if (strstr(name, "Idle") || strstr(name, "idle"))
      return name;
  }
  return first;
}

int BLB_TestSound(void) {
  BLB_TestContext app;
  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Sound Test", HMM_V3(0.0f, 0.0f, 13.0f), 9, 10, 16) != 0)
    return -1;

  BLB_TestAddStudioLights(&app, 36.0f);
  BLB_Skybox *skybox = BLB_Skybox_Load2D("assets/tests/textures/sky_17_2k.png");
  BLB_IO_Audio *io_audio = BLB_InitAudio();
  if (!io_audio) {
    if (skybox)
      BLB_Skybox_Destroy(skybox);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Audio3D *audio3d = BLB_LoadAudio3D(io_audio, "assets/tests/sounds/blackpensil.mp3", true, true, true, HMM_V3(0.0f, 0.0f, -300.0f));

  BLB_Object3D *g =
      BLB_LoadModelFBX("assets/tests/models/adventurer.fbx", HMM_V3(0.05f, 0.05f, 0.05f), HMM_V3(0.0f, 0.0f, -300.0f), NULL, BLB_TEXMAP_STRETCH);

  // BLB_Object3D *g = BLB_LoadModelGLB("assets/tests/models/music_player.glb", HMM_V3(4.4f, 4.4f, 4.4f), HMM_V3(0, 0, -300), NULL,
  // BLB_TEXMAP_STRETCH);

  // BLB_Object3D *g = BLB_LoadModelOBJ("assets/tests/models/music_player.obj", HMM_V3(4.4f, 4.4f, 4.4f), HMM_V3(0, 0, -300), NULL,
  // BLB_TEXMAP_STRETCH);

  // BLB_Object3D_SetMaterialFromFile(g, "assets/tests/models/music_player.mtl");

  BLB_Object3D_SetRotation(g, HMM_V3(-1, -10, -5));

  if (app.light_count > 0 && app.lights[0])
    app.lights[0]->enabled = false;
  (void)BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(0.0f, 5.0f, -294.0f), HMM_V3(0, 0, 0), 16.0f, 0.08f, 1.0f, 48.0f);
  (void)BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(-5.0f, -2.0f, -296.0f), HMM_V3(0, 0, 0), 8.0f, 0.05f, 0.8f, 48.0f);
  (void)BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(5.0f, -1.0f, -296.0f), HMM_V3(0, 0, 0), 8.0f, 0.05f, 0.8f, 48.0f);

  if (!audio3d) {
    if (g)
      BLB_DestroyModelImported(g);
    if (skybox)
      BLB_Skybox_Destroy(skybox);
    BLB_ShutdownAudio(io_audio);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_SetAudio3DSeek(audio3d, 30, 0);
  BLB_AddAudio3D(app.scene, audio3d);
  if (g && BLB_AddObject3D(app.scene, g) != 0) {
    BLB_DestroyModelImported(g);
    g = NULL;
  }

  const char *clip = sound_test_animation_clip(g);
  if (g && clip)
    (void)BLB_Model_PlayAnimation(g, clip, true);

  if (skybox) {
    BLB_SetSceneSkybox(app.scene, skybox);
    BLB_Skybox_SetFovScale(skybox, 0.001f);
  }

  while (!BLB_WindowShouldClose(app.window)) {
    float dt = 0.0f;
    int frame = BLB_TestContext_BeginFrame(&app, &dt);
    if (frame < 0)
      break;
    if (frame > 0)
      continue;

    if (g && BLB_Model_GetAnimationCount(g) > 0)
      BLB_Model_UpdateAnimation(g, dt);

    if (app.camera->position.z > -280.0f) {
      BLB_Camera_Move(app.camera, HMM_V3(0.0f, 0.0f, -40.0f));
    } else {
      // BLB_Camera_Rotate(app.camera, HMM_V3(40, 40, 0));
    }

    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  BLB_DestroyScene(app.scene);
  app.scene = NULL;
  BLB_DestroyAudio3D(io_audio, audio3d);
  BLB_ShutdownAudio(io_audio);
  if (g)
    BLB_DestroyModelImported(g);
  BLB_TestContext_Shutdown(&app);
  return 0;
}
