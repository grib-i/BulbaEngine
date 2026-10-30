#include "test.h"

static void vectors_and_matrices(void) {
  BLB_Vec3 a = BLB_V3(1, 2, 3), b = BLB_V3(4, 5, 6);
  BLB_Vec3 sum = BLB_AddV3(a, b);

  TEST_NEAR(sum.x, 5, 1e-6);
  TEST_NEAR(sum.y, 7, 1e-6);
  TEST_NEAR(sum.z, 9, 1e-6);
  TEST_NEAR(BLB_DotV3(a, b), 32, 1e-6);
  TEST_NEAR(BLB_LenV3(BLB_NormV3(a)), 1, 1e-5);

  BLB_Vec3 cross = BLB_Cross(BLB_V3(1, 0, 0), BLB_V3(0, 1, 0));

  TEST_NEAR(cross.z, 1, 1e-6);
  BLB_Mat4 identity = BLB_M4D(1);
  BLB_Vec4 result = BLB_MulM4V4(identity, BLB_V4(2, 3, 4, 1));

  TEST_NEAR(result.x, 2, 1e-6);
  TEST_NEAR(result.w, 1, 1e-6);

  BLB_Quat q = BLB_QuatFromEuler(BLB_V3(0, 0, 0));

  TEST_NEAR(q.w, 1, 1e-6);
  TEST_NEAR(BLB_EulerFromQuat(q).x, 0, 1e-6);
  TEST_CHECK(BLB_MaxInt(-5, 2, 10, 3, INT_MIN) == 10);
  TEST_CHECK(BLB_MinInt(-5, 2, 10, 3, INT_MAX) == -5);
}

static void camera_transforms(void) {
  BLB_Camera *camera = BLB_CreateCamera(BLB_V3(1, 2, 3));

  TEST_REQUIRE(camera);
  BLB_Camera_SetPosition(camera, BLB_V3(4, 5, 6));
  BLB_Camera_SetRotation(camera, BLB_V3(10, 20, 30));
  TEST_NEAR(camera->position.z, 6, 1e-6);
  TEST_NEAR(camera->rotation.y, 20, 1e-6);

  BLB_Mat4 view = BLB_CameraView(camera);
  BLB_Mat4 projection = BLB_CameraProjection(camera, 16.0f / 9.0f);

  TEST_CHECK(isfinite(view.Elements[0][0]));
  TEST_CHECK(isfinite(projection.Elements[0][0]));
  BLB_DestroyCamera(camera);
  BLB_DestroyCamera(NULL);
}

static void delta_and_scene_configuration(void) {
  uint64_t revision = BLB_DeltaTime_GetRevision();

  BLB_DeltaTime_Set(1.0f / 60.0f);
  TEST_NEAR(BLB_DeltaTime_Get(), 1.0f / 60.0f, 1e-6);
  TEST_CHECK(BLB_DeltaTime_GetRevision() > revision);

  BLB_Scene *scene = BLB_CreateScene("configuration");

  TEST_REQUIRE(scene);
  TEST_CHECK(!strcmp(scene->name, "configuration"));
  BLB_SetSceneEnabled(scene, false);
  BLB_SetSceneVisible(scene, false);
  BLB_SetSceneLayer(scene, 12);
  BLB_SetSceneClear(scene, true, BLB_ColorRGBA(1, 2, 3, 255));
  TEST_CHECK(!scene->enabled && !scene->visible && scene->layer == 12);
  TEST_CHECK(scene->clear_color.b == 3);

  BLB_SetSceneDeltaTime(scene, 2.0f);
  TEST_NEAR(scene->delta_time, 0.1, 1e-6);
  BLB_SetSceneDeltaTime(scene, -1.0f);
  TEST_NEAR(scene->delta_time, 0, 1e-6);
  uint64_t frame = scene->frame_index;

  BLB_SetSceneDeltaTime(scene, NAN);
  TEST_CHECK(scene->frame_index == frame);
  BLB_DestroyScene(scene);
  BLB_DestroyScene(NULL);
}

static void scene_add_remove(void) {
  BLB_Scene *scene = BLB_CreateScene("entries");

  TEST_REQUIRE(scene);
  BLB_Object2D *a = BLB_CreateSquare2D(BLB_V2(1, 1), BLB_V2(0, 0), NULL, false);
  BLB_Object3D *b = BLB_CreateCube3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);

  TEST_CHECK(a && b);

  if (a && b) {
    TEST_CHECK(BLB_AddObject2D(scene, a) == 0);
    TEST_CHECK(BLB_AddObject3D(scene, b) == 0);
    TEST_CHECK(a->entity_id && b->entity_id && a->entity_id != b->entity_id);
    TEST_CHECK(scene->object2d_count == 1 && scene->object3d_count == 1);
    TEST_CHECK(BLB_RemoveObject2D(scene, a) == 0);
    TEST_CHECK(BLB_RemoveObject3D(scene, b) == 0);
    TEST_CHECK(BLB_RemoveObject2D(scene, a) == -1);
    TEST_CHECK(scene->object2d_count == 0 && scene->object3d_count == 0);
    TEST_CHECK(BLB_AddObject2D(NULL, a) == -1);
    TEST_CHECK(BLB_AddObject3D(scene, NULL) == -1);
  }

  Test_Destroy2D(a);
  Test_Destroy3D(b);
  BLB_DestroyScene(scene);
}

static void shapes_and_transforms(void) {
  BLB_Object2D *square = BLB_CreateSquare2D(BLB_V2(2, 3), BLB_V2(4, 5), NULL, true);
  BLB_Object2D *circle = BLB_CreateCircle2D(BLB_V2(2, 2), BLB_V2(0, 0), 24, NULL, false);
  BLB_Object3D *cube = BLB_CreateCube3D(BLB_V3(2, 3, 4), BLB_V3(1, 2, 3), NULL, BLB_TEXMAP_UV);
  BLB_Object3D *sphere = BLB_CreateSphere3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), 16, NULL, BLB_TEXMAP_UV);
  BLB_Object3D *torus = BLB_CreateTorus3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), 16, 1, 0.4f, NULL, BLB_TEXMAP_UV);

  TEST_CHECK(square && circle && cube && sphere && torus);

  if (square && circle && cube && sphere && torus) {
    TEST_CHECK(square->polygon->index_count == 6);
    TEST_CHECK(circle->polygon->vertex_count >= 3);
    TEST_CHECK(cube->polygon->index_count == 36);
    TEST_CHECK(sphere->polygon->index_count > 0 && torus->polygon->index_count > 0);
    BLB_Object2D_Transform(square, BLB_V2(7, 8), 45, BLB_V2(6, 9));
    TEST_NEAR(square->position.x, 7, 1e-6);
    TEST_NEAR(square->rotation, 45, 1e-6);
    TEST_NEAR(square->scale.y, 9, 1e-6);
    BLB_Object2D_SetCameraAttached(square, false);
    TEST_CHECK(!BLB_Object2D_IsCameraAttached(square));
    BLB_Object2D_SetColor(square, BLB_ColorRGBA(10, 20, 30, 255));
    square->visible = false;
    square->layer = 7;
    TEST_CHECK(!square->visible && square->layer == 7);
    TEST_NEAR(square->color.g, 20, 1e-6);

    BLB_Object3D_Transform(cube, BLB_V3(7, 8, 9), BLB_V3(10, 20, 30), BLB_V3(3, 4, 5));
    TEST_NEAR(cube->position.z, 9, 1e-6);
    TEST_NEAR(cube->scale.y, 4, 1e-6);
    BLB_Object3D_SetCameraAttached(cube, true);
    TEST_CHECK(BLB_Object3D_IsCameraAttached(cube));
    BLB_Object3D_SetColor(cube, BLB_ColorRGBA(20, 40, 60, 255));
    TEST_CHECK(cube->color.r == 20 && cube->color.g == 40 && cube->color.b == 60 && cube->color.a == 255);
    BLB_Object3D_SetLayer(cube, 9);
    BLB_Object3D_SetVisible(cube, false);
    TEST_CHECK(!cube->visible && cube->layer == 9);
  }

  Test_Destroy2D(square);
  Test_Destroy2D(circle);
  Test_Destroy3D(cube);
  Test_Destroy3D(sphere);
  Test_Destroy3D(torus);
}

static void relative_transforms_and_invalid_values(void) {
  BLB_Object2D *a = BLB_CreateSquare2D(BLB_V2(1, 1), BLB_V2(0, 0), NULL, false);
  BLB_Object3D *b = BLB_CreateCube3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);

  TEST_CHECK(a && b);
  if (a && b) {
    a->delta_time = b->delta_time = 0.5f;
    BLB_Object2D_Move(a, BLB_V2(2, 4));
    BLB_Object2D_Rotate(a, 90);
    BLB_Object2D_Scale(a, BLB_V2(2, 4));
    TEST_NEAR(a->position.x, 1, 1e-5);
    TEST_NEAR(a->rotation, 45, 1e-5);
    TEST_NEAR(a->scale.y, 3, 1e-5);
    BLB_Object2D_FlipX(a);
    BLB_Object2D_FlipY(a);
    TEST_NEAR(a->scale.x, -2, 1e-5);
    TEST_NEAR(a->scale.y, -3, 1e-5);
    BLB_Object2D_SetPosition(a, BLB_V2(NAN, 1));
    TEST_NEAR(a->position.x, 1, 1e-5);

    BLB_Object3D_Move(b, BLB_V3(2, 4, 6));
    BLB_Object3D_Rotate(b, BLB_V3(20, 40, 60));
    BLB_Object3D_Scale(b, BLB_V3(2, 4, 6));
    TEST_NEAR(b->position.z, 3, 1e-5);
    TEST_NEAR(b->rotation.y, 20, 1e-5);
    TEST_NEAR(b->scale.z, 4, 1e-5);
    BLB_Object3D_FlipX(b);
    BLB_Object3D_FlipY(b);
    BLB_Object3D_FlipZ(b);
    TEST_NEAR(b->scale.z, -4, 1e-5);
    BLB_Object3D_SetPosition(b, BLB_V3(INFINITY, 0, 0));
    TEST_NEAR(b->position.x, 1, 1e-5);
    BLB_Object3D_SetRoundness(b, 0.3f);
    TEST_NEAR(b->roundness, 0.3, 1e-5);
  }
  Test_Destroy2D(a);
  Test_Destroy3D(b);
}

static void fps_and_platform_utilities(void) {
  BLB_FPS_SetLimit(120);
  TEST_CHECK(BLB_FPS_GetLimit() == 120);
  BLB_FPS_SetLimit(-1);
  TEST_CHECK(BLB_FPS_GetLimit() == 0);
  BLB_UpdateFPS();
  BLB_UpdateFPS();
  TEST_CHECK(isfinite(BLB_FPS_GetValue()) && BLB_FPS_GetValue() >= 0);
  TEST_CHECK(BLB_FPS_SetVSync(NULL, true) == -1);
  char *copy = BLB_Platform_DuplicateString("copy");

  TEST_CHECK(copy && !strcmp(copy, "copy"));
  free(copy);
  double before = BLB_Platform_TimeSeconds();

  BLB_Platform_SleepSeconds(0.001);
  TEST_CHECK(BLB_Platform_TimeSeconds() >= before);
}

static const Test_Case core_cases[] = {TEST_CASE(relative_transforms_and_invalid_values),
                                       TEST_CASE(fps_and_platform_utilities),
                                       TEST_CASE(vectors_and_matrices),
                                       TEST_CASE(camera_transforms),
                                       TEST_CASE(delta_and_scene_configuration),
                                       TEST_CASE(scene_add_remove),
                                       TEST_CASE(shapes_and_transforms)};
const Test_Suite test_core = {"core", core_cases, sizeof(core_cases) / sizeof(core_cases[0])};
