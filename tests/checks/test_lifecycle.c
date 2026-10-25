#include "test.h"

int Test_Lifecycle(const Test_Settings *settings) {
  Test_Settings config = settings ? *settings : Test_DefaultSettings();

  if (!config.cycles || config.cycles > 1000000 || !config.ticks || config.ticks > 100000)
    return EXIT_FAILURE;

  Test_Initialize();
  const size_t types = BLB_OBJECTS_ID_COUNT;
  uint32_t *baseline = calloc(types, sizeof(*baseline));

  if (!baseline)
    return EXIT_FAILURE;

  for (size_t i = 0; i < types; ++i)
    baseline[i] = BLB_OBJECTS_ID[i].id;

  bool success = true;
  double started = BLB_Platform_TimeSeconds();

  for (unsigned cycle = 0; cycle < config.cycles; ++cycle) {
    BLB_Scene *scene = BLB_CreateScene("lifecycle");
    BLB_Object2D *square = BLB_CreateSquare2D(BLB_V2(1, 1), BLB_V2(0, 0), NULL, false);
    BLB_Object3D *cube = BLB_CreateCube3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);
    BLB_ParticleSystem2D *particles = BLB_CreateParticleSystem2D(BLB_V2(1, 1), BLB_V2(0, 0), 32, 64, 1, 0, NULL);
    BLB_Material *material = BLB_Material_Create3D();

    success &= scene && square && cube && particles && material;

    if (scene && square && cube && particles && material) {
      success &= BLB_AddObject2D(scene, square) == 0;
      success &= BLB_AddObject3D(scene, cube) == 0;
      success &= BLB_AddParticleSystem2D(scene, particles) == 0;
      BLB_Object3D_SetMaterial(cube, material);
      BLB_Physics2D_SetBody(scene->physics_world2d, square, BLB_PHYSICS2D_DYNAMIC);
      BLB_Physics3D_SetBody(scene->physics_world3d, cube, BLB_PHYSICS3D_DYNAMIC);
      BLB_Physics2DShape *shape2d = BLB_Physics2D_CreateShapeFromObject2D(square);
      BLB_Physics3DShape *shape3d = BLB_Physics3D_CreateShape(BLB_PHYSICS3D_SHAPE_BOX, cube->scale);

      success &= shape2d && shape3d;
      if (shape2d)
        success &= BLB_Physics2D_SetCollider(square, shape2d) != NULL;
      if (shape3d)
        success &= BLB_Physics3D_SetCollider(cube, shape3d) != NULL;
      BLB_Physics2D_DestroyShape(shape2d);
      BLB_Physics3D_DestroyShape(shape3d);

      BLB_DeltaTime_Set(1.0f / 60.0f);

      for (unsigned frame = 0; frame < config.ticks; ++frame) {
        BLB_Physics2D_WorldUpdate(scene->physics_world2d);
        BLB_Physics3D_WorldUpdate(scene->physics_world3d);
        BLB_ParticleSystem2D_Update(particles, 1.0f / 60.0f);
      }

      BLB_RemoveObject2D(scene, square);
      BLB_RemoveObject3D(scene, cube);
      BLB_RemoveParticleSystem2D(scene, particles);
      success &= scene->object2d_count == 0 && scene->object3d_count == 0 && scene->particle_system2d_count == 0;
    }

    /* Entries own their resources; scene destruction alone does not free entries. */
    Test_Destroy2D(square);
    Test_Destroy3D(cube);
    BLB_DestroyParticleSystem2D(particles);
    BLB_Material_Release(material);
    BLB_DestroyScene(scene);
  }

  for (size_t i = 0; i < types; ++i)
    success &= BLB_OBJECTS_ID[i].id == baseline[i];

  free(baseline);
  BLB_ObjectLODClearCache();
  printf("[%s] %u lifecycle cycles in %.3f ms\n", success ? "PASS" : "FAIL", config.cycles, (BLB_Platform_TimeSeconds() - started) * 1000.0);
  printf("Leak detection requires ASan/LSan or Valgrind; counters alone do not prove absence of leaks.\n");
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
