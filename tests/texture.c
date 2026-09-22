#include "tests.h"
#include "test_common.h"

int BLB_TestTexture(void) {
  BLB_TestContext app;
  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Texture Test", HMM_V3(0.0f, 0.0f, 13.0f), 9, 10, 16) != 0)
    return -1;

  BLB_TestAddStudioLights(&app, 36.0f);

  BLB_Texture *potato = BLB_Texture_Load2D("assets/tests/textures/potato.png");
  BLB_Texture *stones = BLB_Texture_Load2D("assets/tests/textures/zta-stones.png");

  if (!potato || !stones) {
    BLB_Texture_Release(potato);
    BLB_Texture_Release(stones);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Object3D *sphere = BLB_CreateSphere3D(HMM_V3(3.0f, 3.0f, 3.0f), HMM_V3(-4.5f, 0.0f, 0.0f), 5, potato);
  BLB_Object3D *cube = BLB_CreateCube3D(HMM_V3(3.1f, 3.1f, 3.1f), HMM_V3(0.0f, 0.0f, 0.0f), potato);
  BLB_Object3D *torus = BLB_CreateTorus3D(HMM_V3(2.5f, 2.5f, 2.5f), HMM_V3(4.5f, 0.0f, 0.0f), 5, 0.0f, 0.0f, stones);

  BLB_Texture_Release(potato);
  BLB_Texture_Release(stones);

  if (!sphere || !cube || !torus) {
    BLB_DestroySphere3D(sphere);
    BLB_DestroyCube3D(cube);
    BLB_DestroyTorus3D(torus);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_AddObject3D(app.scene, sphere);
  BLB_AddObject3D(app.scene, cube);
  BLB_AddObject3D(app.scene, torus);

  BLB_Material_SetPBR(sphere->material, 0.05f, 0.45f);
  BLB_Material_SetPBR(cube->material, 0.05f, 0.48f);
  BLB_Material_SetPBR(torus->material, 0.0f, 0.34f);

  while (!BLB_WindowShouldClose(app.window)) {
    float dt = 0.0f;
    int frame = BLB_TestContext_BeginFrame(&app, &dt);
    if (frame < 0)
      break;
    if (frame > 0)
      continue;

    BLB_Object3D_Rotate(sphere, HMM_V3(0.0f, 24.0f, 0.0f));
    BLB_Object3D_Rotate(cube, HMM_V3(8.0f, 18.0f, 5.0f));
    BLB_Object3D_Rotate(torus, HMM_V3(14.0f, 22.0f, 0.0f));

    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  BLB_DestroySphere3D(sphere);
  BLB_DestroyCube3D(cube);
  BLB_DestroyTorus3D(torus);
  BLB_TestContext_Shutdown(&app);
  return 0;
}
