#include "test.h"

static void snapshot_roundtrip(void) {
  /* Value snapshots deliberately have no shared primitive geometry or global object ID.
   * The loader allocates independent snapshot data, not a CreateCube/CreateSquare instance. */
  BLB_Object2D source2d = {0};
  BLB_Object3D source3d = {0};

  source2d.type = BLB_OBJECT_SQUARE;
  source2d.position = BLB_V2(12, 34);
  source2d.scale = BLB_V2(5, 6);
  source2d.rotation = 25;
  source2d.color = BLB_ColorRGBA(50, 100, 150, 255);
  source2d.visible = source2d.camera_attached = true;
  source3d.type = BLB_OBJECT_CUBE;
  source3d.position = BLB_V3(1, 2, 3);
  source3d.scale = BLB_V3(4, 5, 6);
  source3d.rotation = BLB_V3(10, 20, 30);
  source3d.visible = true;
  source3d.color = BLB_ColorRGBA(10, 20, 30, 255);
  source3d.material = BLB_Material_Create3D();
  TEST_REQUIRE(source3d.material);
  BLB_Material_SetBaseColor(source3d.material, BLB_ColorRGBA(40, 80, 120, 200));
  BLB_Material_SetEmission(source3d.material, BLB_ColorRGBA(10, 20, 30, 255), 1.5f);

  char path2d[2048], path3d[2048];
  double serial = BLB_Platform_TimeSeconds() * 1e6;

  snprintf(path2d, sizeof(path2d), "%s/roundtrip-%.0f.b2o", BLB_TEST_OUTPUT_DIR, serial);
  snprintf(path3d, sizeof(path3d), "%s/roundtrip-%.0f.b3o", BLB_TEST_OUTPUT_DIR, serial);
  TEST_CHECK(BLB_Object2D_Save(&source2d, path2d) == 0);
  TEST_CHECK(BLB_Object3D_Save(&source3d, path3d) == 0);
  BLB_Material_Release(source3d.material);
  source3d.material = NULL;
  BLB_Object2D *copy2d = BLB_Object2D_Load(path2d, NULL);
  BLB_Object3D *copy3d = BLB_Object3D_Load(path3d, NULL);

  TEST_CHECK(copy2d && copy3d);

  if (copy2d) {
    TEST_NEAR(copy2d->position.x, 12, 1e-5);
    TEST_NEAR(copy2d->scale.y, 6, 1e-5);
    TEST_NEAR(copy2d->rotation, 25, 1e-5);
    TEST_CHECK(copy2d->camera_attached && copy2d->visible);
    TEST_NEAR(copy2d->color.b, 150, 1e-5);
    /* All pointer fields in this particular value-only snapshot were saved as NULL. */
    TEST_CHECK(!copy2d->polygon && !copy2d->id && !copy2d->material && !copy2d->animation);
    free(copy2d);
  }
  if (copy3d) {
    TEST_NEAR(copy3d->position.z, 3, 1e-5);
    TEST_NEAR(copy3d->rotation.y, 20, 1e-5);
    TEST_NEAR(copy3d->scale.y, 5, 1e-5);
    TEST_CHECK(copy3d->color.r == 10 && copy3d->color.g == 20 && copy3d->color.b == 30 && copy3d->color.a == 255);
    TEST_CHECK(copy3d->material != NULL);
    if (copy3d->material) {
      TEST_CHECK(copy3d->material->base_color.r == 40 && copy3d->material->base_color.g == 80 &&
                 copy3d->material->base_color.b == 120 && copy3d->material->base_color.a == 200);
      TEST_CHECK(copy3d->material->emission_color.r == 10 && copy3d->material->emission_color.g == 20 &&
                 copy3d->material->emission_color.b == 30 && copy3d->material->emission_color.a == 255);
      BLB_Material_Release(copy3d->material);
    }
    TEST_CHECK(!copy3d->polygon && !copy3d->id && !copy3d->mesh.vertices);
    free(copy3d);
  }

  TEST_CHECK(BLB_Object3D_Load(path2d, NULL) == NULL);
  TEST_CHECK(BLB_Object2D_Load(path3d, NULL) == NULL);
  remove(path2d);
  remove(path3d);
  TEST_CHECK(BLB_Object2D_Save(NULL, path2d) == -1);
  TEST_CHECK(BLB_Object3D_Save(NULL, path3d) == -1);
}

static void truncated_snapshot_is_rejected(void) {
  char path[2048];

  snprintf(path, sizeof(path), "%s/truncated-%.0f.b3o", BLB_TEST_OUTPUT_DIR, BLB_Platform_TimeSeconds() * 1e6);
  FILE *file = fopen(path, "wb");
  TEST_REQUIRE(file);
  fputs("bad", file);
  fclose(file);
  TEST_CHECK(BLB_Object3D_Load(path, NULL) == NULL);
  remove(path);
}

static const Test_Case cases[] = {TEST_CASE(snapshot_roundtrip), TEST_CASE(truncated_snapshot_is_rejected)};
const Test_Suite test_io = {"io", cases, sizeof(cases) / sizeof(cases[0])};
