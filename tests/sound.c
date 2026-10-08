#include "bulba/core/camera.h"
#include "bulba/core/objects3d/audio3d.h"
#include "test_common.h"
#include "tests.h"

int BLB_TestSound(void) {
  BLB_TestContext app;

  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Sound Test", HMM_V3(0.0f, 0.0f, 13.0f), 9, 10, 16) != 0)
    return -1;

  BLB_TestAddStudioLights(&app, 36.0f);

  BLB_Skybox *skybox = BLB_Skybox_Load2D("assets/tests/textures/sky_17_2k.png");

  BLB_IO_Audio *io_audio = BLB_InitAudio();
  if (!io_audio) {
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Audio3D *audio3d = BLB_LoadAudio3D(io_audio, "assets/tests/sounds/blackpensil.mp3", true, true, true, HMM_V3(0.0f, 0.0f, -300.0f));

  app.lights[0]->enabled = false;
  if (!audio3d) {
    BLB_ShutdownAudio(io_audio);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  // BLB_SetPosition_Audio3D(audio3d, HMM_V3(0.0f, 0.0f, 6.0f));
  // BLB_SetVolume_Audio3D(audio3d, 1.0f);
  // BLB_SetPitch_Audio3D(audio3d, 1.0f);
  // BLB_StopAudio3D(audio3d);
  BLB_SetAudio3DSeek(audio3d, 30, 0);

  BLB_AddAudio3D(app.scene, audio3d);

  if (skybox)
    BLB_SetSceneSkybox(app.scene, skybox);

  BLB_Skybox_SetFovScale(skybox, 0.001f);

  while (!BLB_WindowShouldClose(app.window)) {
    float dt = 0.0f;

    int frame = BLB_TestContext_BeginFrame(&app, &dt);
    if (frame < 0)
      break;

    if (frame > 0)
      continue;

    // BLB_Camera_Rotate(app.camera, HMM_V3(2, 2, 2));
    if (app.camera->position.z > -290)
      BLB_Camera_Move(app.camera, HMM_V3(0.0f, 0.0f, -40.0f));

    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  BLB_DestroyAudio3D(io_audio, audio3d);
  BLB_ShutdownAudio(io_audio);
  BLB_TestContext_Shutdown(&app);

  return 0;
}
