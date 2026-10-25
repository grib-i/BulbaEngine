#include "bulba/core/objects3d/model_animation_internal.h"
#include "test.h"

static void import_obj_glb_fbx(void) {
  BLB_Object3D *objects[] = {BLB_LoadModelOBJ("assets/tests/models/teapot.obj", BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV),
                             BLB_LoadModelGLB("assets/tests/models/music_player.glb", BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV),
                             BLB_LoadModelFBX("assets/tests/models/adventurer.fbx", BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV)};

  for (size_t i = 0; i < sizeof(objects) / sizeof(objects[0]); ++i) {
    TEST_CHECK(objects[i]);
    if (objects[i]) {
      TEST_CHECK(objects[i]->polygon && objects[i]->polygon->vertex_count > 0);
      TEST_CHECK(objects[i]->polygon->index_count > 0);
      Test_Destroy3D(objects[i]);
    }
  }

  TEST_CHECK(!BLB_LoadModelOBJ("assets/tests/missing.obj", BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV));
}

static void gltf_animation(void) {
  BLB_Object3D *object = BLB_LoadModelGLTF("assets/tests/fixtures/animated.gltf", BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);

  TEST_REQUIRE(object);
  TEST_CHECK(BLB_Model_GetAnimationCount(object) > 0);
  const char *name = BLB_Model_GetAnimationName(object, 0);

  TEST_CHECK(name);
  if (name) {
    TEST_CHECK(BLB_Model_PlayAnimation(object, name, true) == 0);
    BLB_Model_SetAnimationSpeed(object, 1);
    TEST_CHECK(BLB_Model_SetAnimationTime(object, 0.2f) == 0);
    TEST_NEAR(BLB_Model_GetAnimationTime(object), 0.2, 1e-4);
    BLB_Model_UpdateAnimation(object, 0.1f);
    TEST_CHECK(BLB_Model_GetAnimationTime(object) > 0.2f);
    BLB_Model_StopAnimation(object);
  }
  TEST_CHECK(BLB_Model_GetAnimationName(object, SIZE_MAX) == NULL);
  Test_Destroy3D(object);
}

static void texture_asset(void) {
  BLB_Texture *texture = BLB_Texture_Load2D("assets/tests/textures/cc0/red_brick.png");

  TEST_REQUIRE(texture);
  TEST_CHECK(texture->width > 0 && texture->height > 0 && texture->pixel_size > 0);
  BLB_Texture_Release(texture);
}

static void skeletal_deformation(void) {
  const char *paths[] = {"assets/tests/models/fox/Fox.fbx", "assets/tests/models/fox/Fox.gltf"};
  const char *clips[] = {"Run", "Run"};

  for (size_t i = 0; i < 2; ++i) {
    BLB_Object3D *object = i == 0 ? BLB_LoadModelFBX(paths[i], BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV)
                                  : BLB_LoadModelGLTF(paths[i], BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);
    TEST_REQUIRE(object);
    TEST_CHECK(BLB_Model_GetBoneCount(object) > 0);
    TEST_CHECK(BLB_Model_GetAnimationCount(object) >= 3);
    TEST_CHECK(BLB_Model_PlayAnimation(object, clips[i], true) == 0);
    TEST_CHECK(BLB_Model_SetAnimationTime(object, 0) == 0);

    size_t bytes = object->polygon->vertex_count * sizeof(BLB_Vec3);
    BLB_Vec3 *before = malloc(bytes);

    if (!TEST_CHECK(before != NULL)) {
      Test_Destroy3D(object);
      return;
    }

    memcpy(before, object->polygon->vertices, bytes);
    BLB_Model_UpdateAnimation(object, 0.2f);
    TEST_CHECK(memcmp(before, object->polygon->vertices, bytes) != 0);
    TEST_NEAR(BLB_Model_GetAnimationTime(object), 0.2f, 1e-4);

    for (size_t vertex = 0; vertex < object->polygon->vertex_count; ++vertex) {
      BLB_Vec3 value = object->polygon->vertices[vertex];

      TEST_CHECK(isfinite(value.x) && isfinite(value.y) && isfinite(value.z));
    }

    free(before);
    Test_Destroy3D(object);
  }
}

static void character_atlas_frames(void) {
  const char *path = "assets/tests/sprites/adventurer_sheet.png";
  BLB_Texture *first = BLB_SpriteList_Load2D(path, 0, 0, 128, 128, 0);
  BLB_Texture *second = BLB_SpriteList_Load2D(path, 128, 0, 128, 128, 0);

  TEST_CHECK(first && second);

  if (first && second) {
    TEST_CHECK(first->width == 128 && first->height == 128);
    TEST_CHECK(first->pixel_size == second->pixel_size);
    TEST_CHECK(memcmp(first->pixels, second->pixels, first->pixel_size) != 0);
    bool transparent = false, opaque = false;

    for (size_t i = 3; i < first->pixel_size; i += 4) {
      transparent |= first->pixels[i] == 0;
      opaque |= first->pixels[i] == 255;
    }

    TEST_CHECK(transparent && opaque);
  }

  BLB_Texture_Release(first);
  BLB_Texture_Release(second);
}

static void gltf_uv_orientation(void) {
  BLB_Object3D *object = BLB_LoadModelGLTF("assets/tests/models/fox/Fox.gltf", BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);
  TEST_REQUIRE(object);

  /* Reference: Fox.gltf POSITION/TEXCOORD_0 accessor element zero.
   * Search by source position so Assimp's vertex reordering does not affect the test. */
  bool found = false;
  for (size_t mesh = 0; mesh < object->submesh_count; ++mesh) {
    BLB_Polygon3D *polygon = object->submeshes[mesh].polygon;

    for (size_t i = 0; i < polygon->vertex_count; ++i) {
      BLB_Vec3 value = polygon->base_vertices[i];
      if (fabsf(value.x - 2.05637288f) > 1e-4f || fabsf(value.y - 35.21442032f) > 1e-4f || fabsf(value.z + 23.04511833f) > 1e-4f)
        continue;

      /* The position occurs twice at a UV seam; select the reference U. */
      if (fabsf(polygon->uvs[i].x - 0.52871197f) > 1e-4f)
        continue;

      TEST_NEAR(polygon->uvs[i].y, 0.67855197f, 1e-4f);
      found = true;
    }
  }

  TEST_CHECK(found);
  Test_Destroy3D(object);
}

static void independent_skeletal_instances(void) {
  const char *paths[] = {"assets/tests/models/fox/Fox.fbx", "assets/tests/models/fox/Fox.gltf"};

  for (unsigned format = 0; format < 2; ++format) {
    BLB_Object3D *objects[2] = {0};

    for (unsigned i = 0; i < 2; ++i) {
      objects[i] = format == 0 ? BLB_LoadModelFBX(paths[format], BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV)
                               : BLB_LoadModelGLTF(paths[format], BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);
      TEST_REQUIRE(objects[i]);
      TEST_CHECK(BLB_Model_PlayAnimation(objects[i], "Run", true) == 0);
    }

    TEST_CHECK(objects[0]->polygon->vertices != objects[1]->polygon->vertices);
    BLB_Model_UpdateAnimation(objects[0], 0.1f);
    BLB_Model_UpdateAnimation(objects[1], 0.3f);
    TEST_NEAR(BLB_Model_GetAnimationTime(objects[0]), 0.1f, 1e-5);
    TEST_NEAR(BLB_Model_GetAnimationTime(objects[1]), 0.3f, 1e-5);
    TEST_CHECK(memcmp(objects[0]->polygon->vertices, objects[1]->polygon->vertices, objects[0]->polygon->vertex_count * sizeof(BLB_Vec3)) != 0);

    Test_Destroy3D(objects[0]);
    BLB_Model_UpdateAnimation(objects[1], 0.1f);
    TEST_NEAR(BLB_Model_GetAnimationTime(objects[1]), 0.4f, 1e-5);
    TEST_CHECK(BLB_Model_GetBoneCount(objects[1]) == 24);
    Test_Destroy3D(objects[1]);
  }
}

static void starfield_panorama(void) {
  BLB_Texture *texture = BLB_Texture_Load2D("assets/tests/textures/starmap_2020_2k.png");

  TEST_REQUIRE(texture);
  TEST_CHECK(texture->width == 2048 && texture->height == 1024);
  TEST_CHECK(texture->width == texture->height * 2);
  BLB_Texture_Release(texture);
}

static void fbx_fractional_clip_ranges(void) {
  BLB_Object3D *object = BLB_LoadModelFBX("assets/tests/models/fox/Fox.fbx", BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);
  const char *names[] = {"Survey", "Walk", "Run"};
  const double durations[] = {3.416666746, .708333313, 1.158333302};

  TEST_REQUIRE(object);
  for (size_t i = 0; i < 3; ++i) {
    TEST_CHECK(BLB_Model_PlayAnimation(object, names[i], true) == 0);
    TEST_NEAR(BLB_Model_GetAnimationDuration(object), durations[i], 1e-5);
    TEST_CHECK(BLB_Model_SetAnimationTime(object, (float)durations[i] - .01f) == 0);
    BLB_Model_UpdateAnimation(object, .02f);
    TEST_NEAR(BLB_Model_GetAnimationTime(object), .01, 1e-5);
  }

  Test_Destroy3D(object);
}

static void fbx_rotation_matches_source_quaternions(void) {
  BLB_Object3D *fbx = BLB_LoadModelFBX("assets/tests/models/fox/Fox.fbx", BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);
  BLB_Object3D *gltf = BLB_LoadModelGLTF("assets/tests/models/fox/Fox.gltf", BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);
  const char *clips[] = {"Survey", "Walk", "Run"};

  TEST_REQUIRE(fbx && gltf);
  BLB_ModelAnimationInstance *a = fbx->animation3d;
  BLB_ModelAnimationInstance *b = gltf->animation3d;

  TEST_REQUIRE(a && b);
  for (size_t clip = 0; clip < 3; ++clip) {
    TEST_CHECK(BLB_Model_PlayAnimation(fbx, clips[clip], false) == 0);
    TEST_CHECK(BLB_Model_PlayAnimation(gltf, clips[clip], false) == 0);
    float duration = BLB_Model_GetAnimationDuration(gltf);

    for (unsigned frame = 0; frame <= 97; ++frame) {
      float time = duration * frame / 97;

      BLB_Model_SetAnimationTime(fbx, time);
      BLB_Model_SetAnimationTime(gltf, time);
      for (size_t i = 0; i < a->asset->node_count; ++i) {
        for (size_t j = 0; j < b->asset->node_count; ++j) {
          if (strcmp(a->asset->nodes[i].name, b->asset->nodes[j].name))
            continue;

          BLB_Quat q = a->rotations[i], r = b->rotations[j];
          double dot = fabs(q.x * r.x + q.y * r.y + q.z * r.z + q.w * r.w);

          TEST_CHECK(dot > .9998);
        }
      }
    }
  }

  Test_Destroy3D(fbx);
  Test_Destroy3D(gltf);
}

static const Test_Case cases[] = {TEST_CASE(fbx_rotation_matches_source_quaternions),
                                  TEST_CASE(fbx_fractional_clip_ranges),
                                  TEST_CASE(import_obj_glb_fbx),
                                  TEST_CASE(gltf_animation),
                                  TEST_CASE(texture_asset),
                                  TEST_CASE(skeletal_deformation),
                                  TEST_CASE(character_atlas_frames),
                                  TEST_CASE(gltf_uv_orientation),
                                  TEST_CASE(independent_skeletal_instances),
                                  TEST_CASE(starfield_panorama)};
const Test_Suite test_assets = {"assets", cases, sizeof(cases) / sizeof(cases[0])};
