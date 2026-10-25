#include "test.h"

static void emission_motion_and_lifetime(void) {
  BLB_ParticleSystem3D *system = BLB_CreateParticleSystem3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), 0, 16, 1, 0, NULL);

  TEST_REQUIRE(system);
  system->velocity = BLB_V3(2, 0, 0);
  system->gravity = BLB_V3(0, 0, 0);
  system->velocity_variation = BLB_V3(0, 0, 0);
  system->looping = false;
  system->emission_rate = 0;
  TEST_CHECK(BLB_ParticleSystem3D_Emit(system, 4) == 4);
  TEST_CHECK(system->count == 4);
  BLB_ParticleSystem3D_Update(system, 0.25f);
  TEST_NEAR(system->particles[0].position.x, 0.5, 1e-5);
  TEST_NEAR(system->particles[0].lifetime, 0.25, 1e-5);

  BLB_ParticleSystem3D_Stop(system);
  BLB_ParticleSystem3D_Update(system, 0.25f);
  TEST_NEAR(system->particles[0].lifetime, 0.25, 1e-5);
  BLB_ParticleSystem3D_Play(system);

  for (int i = 0; i < 10; ++i)
    BLB_ParticleSystem3D_Update(system, 0.1f);
  /* Expired particles stay allocated for reuse, but are no longer visible. */
  TEST_CHECK(system->count == 4);

  for (size_t i = 0; i < system->count; ++i)
    TEST_CHECK(!system->particles[i].visible);

  TEST_CHECK(BLB_ParticleSystem3D_SetCount(system, 8));
  TEST_CHECK(system->count == 8 && system->capacity >= system->count);
  BLB_ParticleSystem3D_SetCameraAttached(system, true);
  TEST_CHECK(BLB_ParticleSystem3D_IsCameraAttached(system));
  BLB_ParticleSystem3D_Restart(system);
  TEST_CHECK(system->playing);
  BLB_ParticleSystem3D_Clear(system);
  TEST_CHECK(system->count == 0);
  BLB_DestroyParticleSystem3D(system);
}

static const Test_Case cases[] = {TEST_CASE(emission_motion_and_lifetime)};
const Test_Suite test_particles3d = {"particles3d", cases, sizeof(cases) / sizeof(cases[0])};
