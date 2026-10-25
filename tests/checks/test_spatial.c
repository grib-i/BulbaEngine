#include "../demos/demo_common.h"

static void raycast_nearest_and_filter(void) {
  BLB_Scene *scene = BLB_CreateScene("raycasts");

  TEST_REQUIRE(scene);
  BLB_Object3D *near =
      BLB_CreateCube3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);
  BLB_Object3D *far =
      BLB_CreateCube3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, -3), NULL, BLB_TEXMAP_UV);

  TEST_CHECK(near && far);
  if (near && far) {
    BLB_AddObject3D(scene, near);
    BLB_AddObject3D(scene, far);
    BLB_RayHit3D **hits = BLB_Ray3D(scene, BLB_V3(0, 0, 5), BLB_V3(0, 0, -10),
                                    BLB_RAY_FIRST_HIT, NULL);

    TEST_CHECK(hits && hits[0]);
    if (hits && hits[0]) {
      TEST_CHECK(hits[0]->object == near);
      // BLB_Ray3D currently uses position +/- scale as its broad-phase AABB.
      TEST_NEAR(hits[0]->distance, 4.0, 0.01);
    }
    BLB_RayHits3D_Destroy(hits);
    hits = BLB_Ray3D(scene, BLB_V3(0, 0, 5), BLB_V3(0, 0, -10), BLB_RAY_FILTER,
                     near, NULL);
    TEST_CHECK(hits && hits[0] && hits[0]->object == far);
    BLB_RayHits3D_Destroy(hits);
    hits = BLB_Ray3D(scene, BLB_V3(20, 0, 5), BLB_V3(20, 0, -10),
                     BLB_RAY_ALL_HITS, NULL);
    TEST_CHECK(hits == NULL);
    BLB_RayHits3D_Destroy(hits);
    BLB_RemoveObject3D(scene, near);
    BLB_RemoveObject3D(scene, far);
  }
  Test_Destroy3D(near);
  Test_Destroy3D(far);
  BLB_DestroyScene(scene);
}

static void lights_and_lod(void) {
  BLB_Light3D *light3d = BLB_CreateLight3D(BLB_LIGHT_DIRECTIONAL,
                                           BLB_V3(0, 5, 0), BLB_V3(0, 0, 0));
  BLB_Light2D *light2d = BLB_CreateLight2D(BLB_LIGHT_POINT, BLB_V2(0, 0), 0);

  TEST_CHECK(light3d && light2d);
  if (light3d) {
    TEST_CHECK(light3d->enabled);
    TEST_NEAR(BLB_LenV3(BLB_GetLightDirection3D(light3d)), 1, 1e-5);
  }
  BLB_DestroyLight3D(light3d);
  BLB_DestroyLight2D(light2d);

  BLB_Object3D *sphere = BLB_CreateSphere3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0),
                                            16, NULL, BLB_TEXMAP_UV);

  TEST_REQUIRE(sphere);
  TEST_CHECK(BLB_Object3D_SetLODLevel(sphere, 1));
  TEST_CHECK(BLB_Object3D_GetLODLevel(sphere) == 1);
  TEST_CHECK(BLB_Object3D_GetLODPolygon(sphere));
  BLB_Object3D_ResetLODOverride(sphere);
  TEST_CHECK(!sphere->lod_override);
  Test_Destroy3D(sphere);
  BLB_ObjectLODClearCache();
}

static void rotated_objects_clear_the_stage(void) {
  BLB_Object3D *cube = BLB_CreateCube3D(BLB_V3(2, 4, 6), BLB_V3(5, -10, -3),
                                        NULL, BLB_TEXMAP_UV);
  BLB_Vec3 minimum, maximum;

  TEST_REQUIRE(cube);
  BLB_Object3D_SetRotation(cube, BLB_V3(0, 0, 45));
  TEST_CHECK(Demo_Bounds3D(cube, true, &minimum, &maximum));
  TEST_NEAR(maximum.y - cube->position.y, 3 / sqrt(2), 1e-5);

  for (unsigned angle = 0; angle <= 360; angle += 3) {
    BLB_Object3D_SetRotation(cube, BLB_V3(angle * 0.7f, angle, angle * 0.3f));
    Demo_GroundObject(cube, 0, 0.04f);

    TEST_CHECK(Demo_Bounds3D(cube, true, &minimum, &maximum));
    TEST_NEAR(minimum.y, 0.04f, 1e-5);
    TEST_NEAR(cube->position.x, 5, 1e-5);
    TEST_NEAR(cube->position.z, -3, 1e-5);
  }

  BLB_Object3D_SetScale(cube, BLB_V3(-2, 4, 6));
  Demo_GroundObject(cube, 1.25f, 0.04f);
  TEST_CHECK(Demo_Bounds3D(cube, true, &minimum, &maximum));
  TEST_NEAR(minimum.y, 1.29f, 1e-5);
  Test_Destroy3D(cube);
}

static void animated_clip_envelopes_clear_the_stage(void) {
  const char *paths[] = {"assets/tests/models/fox/Fox.fbx",
                         "assets/tests/models/fox/Fox.gltf"};

  for (unsigned format = 0; format < 2; ++format) {
    BLB_Object3D *object =
        format == 0 ? BLB_LoadModelFBX(paths[format], BLB_V3(1, 1, 1),
                                       BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV)
                    : BLB_LoadModelGLTF(paths[format], BLB_V3(1, 1, 1),
                                        BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);

    TEST_REQUIRE(object);
    TEST_CHECK(!Demo_FitAnimatedModel(object, 4.6f, 961));

    for (size_t clip = 0; clip < BLB_Model_GetAnimationCount(object); ++clip) {
      TEST_CHECK(BLB_Model_PlayAnimation(
                     object, BLB_Model_GetAnimationName(object, clip), true) ==
                 0);
      TEST_CHECK(BLB_Model_SetAnimationTime(object, 0.1f) == 0);
      TEST_CHECK(Demo_FitAnimatedModel(object, 4.6f, 961));
      TEST_NEAR(BLB_Model_GetAnimationTime(object), 0.1f, 1e-4);
      BLB_Vec3 placement = object->position;
      float duration = BLB_Model_GetAnimationDuration(object);

      /* Offset the probe grid from the layout samples to also test in-between
       * poses. */
      for (unsigned frame = 0; frame < 480; ++frame) {
        BLB_Vec3 minimum, maximum;

        TEST_CHECK(BLB_Model_SetAnimationTime(
                       object, duration * (frame + 0.37f) / 480) == 0);
        TEST_CHECK(Demo_Bounds3D(object, true, &minimum, &maximum));
        if (minimum.y < 0.02f)
          fprintf(stderr, "Clip clearance %s / %s: frame %u, y %.8f\n",
                  paths[format], BLB_Model_GetAnimationName(object, clip),
                  frame, minimum.y);
        TEST_CHECK(minimum.y >= 0.02f);
        TEST_NEAR(object->position.y, placement.y, 1e-6);
      }
    }

    Test_Destroy3D(object);
  }

  TEST_NEAR(BLB_Model_GetAnimationDuration(NULL), 0, 1e-6);
}

static const Test_Case cases[] = {
    TEST_CASE(raycast_nearest_and_filter), TEST_CASE(lights_and_lod),
    TEST_CASE(rotated_objects_clear_the_stage),
    TEST_CASE(animated_clip_envelopes_clear_the_stage)};
const Test_Suite test_spatial = {"spatial", cases,
                                 sizeof(cases) / sizeof(cases[0])};
