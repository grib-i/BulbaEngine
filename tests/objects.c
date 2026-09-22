#include "test_common.h"
#include "tests.h"

int BLB_TestObjects(void) {
  BLB_TestContext app;

  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Objects", HMM_V3(0.0f, 0.0f, 15.5f), 7, 9, 15) != 0) {
    return -1;
  }

  BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(0.0f, 1.0f, 6.0f), HMM_V3(0.0f, 60.0f, 0.0f), 13.0f, 0.025f, 1.0f, 32.0f);

  BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(-6.0f, -2.0f, 4.0f), HMM_V3(0.0f, 5.0f, 0.0f), 6.0f, 0.01f, 0.7f, 28.0f);

  BLB_Object3D *sphere = BLB_CreateSphere3D(HMM_V3(2.25f, 2.25f, 2.25f), HMM_V3(-4.5f, 0.0f, 0.0f), 5, NULL);

  BLB_Object3D *cube = BLB_CreateCube3D(HMM_V3(2.35f, 2.35f, 2.35f), HMM_V3(0.0f, 0.0f, 0.0f), NULL);

  BLB_Object3D *torus = BLB_CreateTorus3D(HMM_V3(2.25f, 2.25f, 2.25f), HMM_V3(4.5f, 0.0f, 0.0f), 5, 0.0f, 0.0f, NULL);

  if (sphere) {
    sphere->layer = 1;
    BLB_AddObject3D(app.scene, sphere);
  }

  if (cube) {
    cube->layer = 1;
    cube->rotation = HMM_V3(8.0f, 25.0f, 8.0f);
    BLB_AddObject3D(app.scene, cube);
  }

  if (torus) {
    torus->layer = 1;
    torus->rotation = HMM_V3(30.0f, 0.0f, 0.0f);
    BLB_AddObject3D(app.scene, torus);
  }

  while (!BLB_WindowShouldClose(app.window)) {
    float delta_time = 0.0f;

    int frame = BLB_TestContext_BeginFrame(&app, &delta_time);

    if (frame < 0)
      break;

    if (frame > 0)
      continue;

    if (sphere)
      BLB_Object3D_Rotate(sphere, HMM_V3(0.0f, 22.0f, 0.0f));

    if (cube)
      BLB_Object3D_Rotate(cube, HMM_V3(10.0f, 16.0f, 5.0f));

    if (torus)
      BLB_Object3D_Rotate(torus, HMM_V3(18.0f, 24.0f, 0.0f));

    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  BLB_DestroySphere3D(sphere);
  BLB_DestroyCube3D(cube);
  BLB_DestroyTorus3D(torus);

  BLB_TestContext_Shutdown(&app);

  return 0;
}
