#include "bulba/core/objects3d/model_loader.h"
#include "test_common.h"
#include "tests.h"

static BLB_Object3D *create_object(BLB_TestObjectType type, HMM_Vec3 position) {
  switch (type) {
  case BLB_TEST_OBJECT_CUBE:
    return BLB_CreateCube3D(HMM_V3(2.4f, 2.4f, 2.4f), position, NULL);
  case BLB_TEST_OBJECT_SPHERE:
    return BLB_CreateSphere3D(HMM_V3(2.4f, 2.4f, 2.4f), position, 5, NULL);
  case BLB_TEST_OBJECT_TORUS:
    return BLB_CreateTorus3D(HMM_V3(1.6f, 1.6f, 1.6f), position, 5, 0.0f, 0.0f, NULL);
  case BLB_TEST_OBJECT_TEAPOT:
    return BLB_LoadModelOBJ("assets/tests/models/teapot.obj", HMM_V3(0.6f, 0.6f, 0.6f), position, NULL);
  default:
    return NULL;
  }
}

static void destroy_object(BLB_TestObjectType type, BLB_Object3D *object) {
  if (!object)
    return;

  switch (type) {
  case BLB_TEST_OBJECT_CUBE:
    BLB_DestroyCube3D(object);
    break;
  case BLB_TEST_OBJECT_SPHERE:
    BLB_DestroySphere3D(object);
    break;
  case BLB_TEST_OBJECT_TORUS:
    BLB_DestroyTorus3D(object);
    break;
  case BLB_TEST_OBJECT_TEAPOT:
    BLB_DestroyModelOBJ(object);
    break;
  default:
    break;
  }
}

static float object_radius(BLB_TestObjectType type) {
  switch (type) {
  case BLB_TEST_OBJECT_TEAPOT:
    return 1.125f;
  case BLB_TEST_OBJECT_TORUS:
    return 1.2f;
  case BLB_TEST_OBJECT_CUBE:
  case BLB_TEST_OBJECT_SPHERE:
    return 1.2f;
  default:
    return 1.2f;
  }
}

static void fit_camera(BLB_TestContext *app, size_t columns, size_t rows, float spacing, float radius) {
  if (!app || !app->camera || !columns || !rows)
    return;

  float width = (float)(columns - 1) * spacing + radius * 2.0f;
  float height = (float)(rows - 1) * spacing + radius * 2.0f;
  float aspect = 1.0f;

  if (app->vk.swapchain_extent.height > 0)
    aspect = (float)app->vk.swapchain_extent.width / (float)app->vk.swapchain_extent.height;
  if (aspect <= 0.0f)
    aspect = 1.0f;

  float half_vertical_fov = app->camera->fov * 0.5f;
  float tan_vertical = tanf(half_vertical_fov);
  float horizontal_fov = 2.0f * atanf(tan_vertical * aspect);
  float tan_horizontal = tanf(horizontal_fov * 0.5f);

  float distance_vertical = height * 0.5f / tan_vertical;
  float distance_horizontal = width * 0.5f / tan_horizontal;
  float distance = fmaxf(distance_vertical, distance_horizontal) * 1.12f + radius;

  BLB_Camera_SetPosition(app->camera, HMM_V3(0.0f, 0.0f, distance));
  app->camera->far_plane = fmaxf(1000.0f, distance * 4.0f);
}

int BLB_TestObjectCount(size_t count, BLB_TestObjectType type) {
  if (count == 0)
    return -1;

  if (type != BLB_TEST_OBJECT_CUBE && type != BLB_TEST_OBJECT_SPHERE && type != BLB_TEST_OBJECT_TORUS && type != BLB_TEST_OBJECT_TEAPOT)
    return -1;

  BLB_TestContext app;
  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Object Count Test", HMM_V3(0.0f, 0.0f, 15.0f), 6, 7, 11) != 0)
    return -1;

  size_t columns = (size_t)ceil(sqrt((double)count * (1200.0 / 900.0)));
  if (columns < 1)
    columns = 1;

  size_t rows = (count + columns - 1) / columns;
  float spacing = 3.0f;
  float radius = object_radius(type);

  BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(0.0f, 16.0f, 18.0f), HMM_V3(0.0f, 0.0f, 0.0f), 20.0f, 0.05f, 1.0f,
                           fmaxf(80.0f, (float)count * 0.12f));

  BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(-18.0f, -8.0f, 10.0f), HMM_V3(0.0f, 0.0f, 0.0f), 9.0f, 0.025f, 0.6f,
                           fmaxf(80.0f, (float)count * 0.12f));

  BLB_Material *shared_material = BLB_Material_Build(BLB_MATERIAL_3D, NULL);
  if (!shared_material) {
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Material_SetBaseColor(shared_material, 0.58f, 0.66f, 0.78f, 1.0f);
  BLB_Material_SetPBR(shared_material, 0.12f, 0.38f);
  BLB_Material_SetSpecular(shared_material, 0.85f, 0.92f, 0.96f, 1.0f);

  BLB_Object3D **objects = calloc(count, sizeof(*objects));
  if (!objects) {
    BLB_Material_Release(shared_material);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  int error = 0;

  for (size_t i = 0; i < count; ++i) {
    size_t row = i / columns;
    size_t column = i % columns;

    float x = ((float)column - (float)(columns - 1) * 0.5f) * spacing;
    float y = ((float)(rows - 1) * 0.5f - (float)row) * spacing;

    objects[i] = create_object(type, HMM_V3(x, y, 0.0f));

    if (!objects[i]) {
      error = 1;
      break;
    }

    BLB_Object3D_SetMaterial(objects[i], shared_material);

    objects[i]->rotation = HMM_V3((float)((i * 7) % 360), (float)((i * 17) % 360), (float)((i * 5) % 90));

    objects[i]->layer = 1;

    if (BLB_AddObject3D(app.scene, objects[i]) != 0) {
      error = 1;
      break;
    }
  }

  BLB_Material_Release(shared_material);

  if (error) {
    for (size_t i = 0; i < count; ++i)
      destroy_object(type, objects[i]);

    free(objects);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  fit_camera(&app, columns, rows, spacing, radius);

  while (!BLB_WindowShouldClose(app.window)) {
    float dt = 0.0f;
    int frame = BLB_TestContext_BeginFrame(&app, &dt);

    if (frame < 0)
      break;

    if (frame > 0)
      continue;

    fit_camera(&app, columns, rows, spacing, radius);

    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  for (size_t i = 0; i < count; ++i)
    destroy_object(type, objects[i]);

  free(objects);
  BLB_TestContext_Shutdown(&app);

  return 0;
}
